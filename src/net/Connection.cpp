#include "synccanvas/Connection.hpp"
#include <algorithm>
namespace synccanvas {
Connection::Connection(asio::ip::tcp::socket socket, std::size_t incomingBytesPerSecond)
    : socket_(std::move(socket)), deadline_(socket_.get_executor()),
      incomingBytesPerSecond_(incomingBytesPerSecond), byteTokens_(incomingBytesPerSecond * 4.0) {}
void Connection::start(MessageHandler message, CloseHandler closed) {
    onMessage_ = std::move(message);
    onClose_ = std::move(closed);
    asio::error_code error;
    socket_.set_option(asio::ip::tcp::no_delay(true), error);
    armDeadline();
    readHeader();
}
void Connection::armDeadline() {
    deadline_.expires_after(std::chrono::seconds(35));
    deadline_.async_wait([self = shared_from_this()](asio::error_code error) {
        if (!error)
            self->close("Connection timed out");
    });
}
void Connection::readHeader() {
    asio::async_read(
        socket_, asio::buffer(header_), [self = shared_from_this()](asio::error_code error, std::size_t) {
            if (error) {
                self->close("Connection closed");
                return;
            }
            try {
                const auto length = decodeLength(self->header_);
                if (self->incomingBytesPerSecond_) {
                    const auto now = std::chrono::steady_clock::now();
                    const double rate = static_cast<double>(self->incomingBytesPerSecond_);
                    self->byteTokens_ = std::min(
                        rate * 4, self->byteTokens_ +
                                      rate * std::chrono::duration<double>(now - self->lastHeader_).count());
                    self->lastHeader_ = now;
                    if (self->byteTokens_ < length + 4.0) {
                        self->close("Incoming byte rate exceeded");
                        return;
                    }
                    self->byteTokens_ -= length + 4.0;
                }
                self->body_.resize(length);
                self->readBody();
            } catch (const std::exception &) {
                self->close("Invalid frame header");
            }
        });
}
void Connection::readBody() {
    asio::async_read(socket_, asio::buffer(body_),
                     [self = shared_from_this()](asio::error_code error, std::size_t) {
                         if (error) {
                             self->close("Connection closed");
                             return;
                         }
                         try {
                             auto message = parseMessage(self->body_);
                             self->armDeadline();
                             auto handler = self->onMessage_;
                             if (handler)
                                 handler(message);
                             if (!self->closed_)
                                 self->readHeader();
                         } catch (const std::exception &) {
                             self->close("Invalid protocol message");
                         }
                     });
}
void Connection::send(const Json &message) {
    if (closed_)
        return;
    auto frame = encodeFrame(message);
    if (queuedBytes_ + frame.size() > 4 * 1024 * 1024 || writes_.size() >= 4096) {
        close("Slow peer exceeded outgoing queue limit");
        return;
    }
    queuedBytes_ += frame.size();
    writes_.push_back(std::move(frame));
    if (writes_.size() == 1)
        write();
}
void Connection::write() {
    asio::async_write(socket_, asio::buffer(writes_.front()),
                      [self = shared_from_this()](asio::error_code error, std::size_t) {
                          if (error) {
                              self->close("Write failed");
                              return;
                          }
                          self->queuedBytes_ -= self->writes_.front().size();
                          self->writes_.pop_front();
                          if (!self->writes_.empty() && !self->closed_)
                              self->write();
                      });
}
void Connection::close(const std::string &reason) {
    if (closed_)
        return;
    closed_ = true;
    asio::error_code ignored;
    deadline_.cancel();
    socket_.cancel(ignored);
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
    // Keep write buffers alive until cancellation handlers release this object.
    auto handler = std::move(onClose_);
    onMessage_ = {};
    if (handler)
        asio::post(socket_.get_executor(), [handler = std::move(handler), reason] { handler(reason); });
}
} // namespace synccanvas
