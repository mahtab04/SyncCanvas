#pragma once
#include "ClientView.hpp"
#include "NetworkClient.hpp"
#include <deque>
#include <filesystem>
struct GLFWwindow;
namespace synccanvas {
class Application {
  public:
    explicit Application(std::array<std::string, 5> fields);
    int run();

  private:
    NetworkClient network_;
    ClientView view_;
    ClientLayout layout_;
    Renderer renderer_;
    GLFWwindow *window_ = nullptr;
    bool keys_[350]{};
    bool keyEvents_[350]{}, undoRequested_ = false;
    bool exportRequested_ = false, importRequested_ = false;
    int requestedTool_ = -1;
    std::string offlineReason_;
    double lastCursor_ = 0, nextReconnect_ = 0;
    double pointerX_ = 0, pointerY_ = 0;
    bool snapshotting_ = false;
    std::deque<Shape> importQueue_;
    double lastImport_ = 0;
    double lastPreview_ = 0;
    bool previewDirty_ = false;
    std::filesystem::path exportPath_ = "board.json";
    bool pressed(int key);
    void input();
    void pointer(double x, double y, int phase);
    void action(const std::string &action);
    void connect();
    void processEvents();
    void exportBoard();
    void importBoard();
    void cancelDraft();
    void resetBoardView();
    void character(unsigned codepoint);
};
} // namespace synccanvas
