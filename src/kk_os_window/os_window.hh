#pragma once
#include "os_window_config.hh"

class OsWindow
{
public:
    OsWindowContext context;
    void* user_data = nullptr;

public:
    explicit OsWindow(OsWindow* main_window = nullptr);
    ~OsWindow();
    OsWindow(const OsWindow& rhs) = delete;
    OsWindow& operator=(const OsWindow& rhs) = delete;
    OsWindow(OsWindow&& rhs) noexcept = delete;
    OsWindow& operator=(OsWindow&& rhs) noexcept = delete;

private:
    OsWindowContext create_context(OsWindow* main_window);
    void destroy_context(const OsWindowContext& os_context);
    void destroy();
};
