#ifndef YALN_EVENT_H
#define YALN_EVENT_H

#include <functional>
#include <map>
#include <vector>
#include <memory>
#include <typeindex>
#include <algorithm>
#include <cstdint>
#include <string>

namespace yaln
{

// ============================================================
// 事件类别枚举（可组合）
// ============================================================
enum class EventCategory : int
{
    None          = 0,
    Application   = 1 << 0,
    Window        = 1 << 1,
    Keyboard      = 1 << 2,
    Mouse         = 1 << 3,
    MouseButton   = 1 << 4,
    Input         = 1 << 5
};

// ============================================================
// 事件类别标志位运算
// ============================================================
inline EventCategory operator|(EventCategory lhs, EventCategory rhs)
{
    return static_cast<EventCategory>(
        static_cast<int>(lhs) | static_cast<int>(rhs)
    );
}

inline EventCategory operator&(EventCategory lhs, EventCategory rhs)
{
    return static_cast<EventCategory>(
        static_cast<int>(lhs) & static_cast<int>(rhs)
    );
}

// ============================================================
// 事件类型枚举
// ============================================================
enum class EventType : uint32_t
{
    None = 0,

    // 窗口事件
    WindowClose,
    WindowResize,
    WindowMove,
    WindowFocus,
    WindowLostFocus,
    WindowRefresh,
    WindowMinimized,
    WindowMaximized,
    WindowRestored,
    WindowScaleChange,

    // 键盘事件
    KeyPressed,
    KeyReleased,
    KeyTyped,
    KeyDown,        // 按键按下（与 KeyPressed 区别：仅首次按下）
    KeyUp,          // 按键释放
    KeyRepeat,      // 按键长按重复
    CharInput,      // 字符输入
    ModifierPressed, // 修饰键按下（Shift/Ctrl/Alt 等）

    // 鼠标事件
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    MouseScrolled,
    MouseEntered,   // 鼠标进入窗口
    MouseLeft,      // 鼠标离开窗口
    MouseDragged,   // 鼠标拖拽

    // 触摸事件
    TouchBegan,
    TouchEnded,
    TouchMoved,
    TouchCancelled,

    // 手柄/摇杆事件
    GamepadConnected,
    GamepadDisconnected,
    GamepadButtonPressed,
    GamepadButtonReleased,
    GamepadAxisMoved,

    // 文件拖放事件
    FileDrop,       // 文件拖入窗口

    // 应用事件
    AppUpdate,
    AppRender,
    AppTick,

    // 自定义事件
    Custom,

    EventTypeCount
};

// ============================================================
// 按键代码枚举（与 GLFW 兼容）
// ============================================================
enum class KeyCode : uint32_t
{
    // 字母键
    Space          = 32,
    Apostrophe     = 39,  // '
    Comma          = 44,  // ,
    Minus          = 45,  // -
    Period         = 46,  // .
    Slash          = 47,  // /
    Key0           = 48,
    Key1           = 49,
    Key2           = 50,
    Key3           = 51,
    Key4           = 52,
    Key5           = 53,
    Key6           = 54,
    Key7           = 55,
    Key8           = 56,
    Key9           = 57,
    Semicolon      = 59,  // ;
    Equal          = 61,  // =
    A              = 65,
    B              = 66,
    C              = 67,
    D              = 68,
    E              = 69,
    F              = 70,
    G              = 71,
    H              = 72,
    I              = 73,
    J              = 74,
    K              = 75,
    L              = 76,
    M              = 77,
    N              = 78,
    O              = 79,
    P              = 80,
    Q              = 81,
    R              = 82,
    S              = 83,
    T              = 84,
    U              = 85,
    V              = 86,
    W              = 87,
    X              = 88,
    Y              = 89,
    Z              = 90,
    LeftBracket    = 91,  // [
    Backslash      = 92,  // backslash
    RightBracket   = 93,  // ]
    GraveAccent    = 96,  // backtick
    World1         = 161, // non-US #1
    World2         = 162, // non-US #2

    // 功能键
    Escape         = 256,
    Enter          = 257,
    Tab            = 258,
    Backspace      = 259,
    Insert         = 260,
    Delete         = 261,
    Right          = 262,
    Left           = 263,
    Down           = 264,
    Up             = 265,
    PageUp         = 266,
    PageDown       = 267,
    Home           = 268,
    End            = 269,
    CapsLock       = 280,
    ScrollLock     = 281,
    NumLock        = 282,
    PrintScreen    = 283,
    Pause          = 284,
    F1             = 290,
    F2             = 291,
    F3             = 292,
    F4             = 293,
    F5             = 294,
    F6             = 295,
    F7             = 296,
    F8             = 297,
    F9             = 298,
    F10            = 299,
    F11            = 300,
    F12            = 301,
    F13            = 302,
    F14            = 303,
    F15            = 304,
    F16            = 305,
    F17            = 306,
    F18            = 307,
    F19            = 308,
    F20            = 309,
    F21            = 310,
    F22            = 311,
    F23            = 312,
    F24            = 313,
    F25            = 314,
    Kp0            = 320,
    Kp1            = 321,
    Kp2            = 322,
    Kp3            = 323,
    Kp4            = 324,
    Kp5            = 325,
    Kp6            = 326,
    Kp7            = 327,
    Kp8            = 328,
    Kp9            = 329,
    KpDecimal      = 330,
    KpDivide       = 331,
    KpMultiply     = 332,
    KpSubtract     = 333,
    KpAdd          = 334,
    KpEnter        = 335,
    KpEqual        = 336,
    LeftShift      = 340,
    LeftControl    = 341,
    LeftAlt        = 342,
    LeftSuper      = 343,
    RightShift     = 344,
    RightControl   = 345,
    RightAlt       = 346,
    RightSuper     = 347,
    Menu           = 348,

    Unknown        = 0xFFFFFFFF
};

// ============================================================
// 修饰键枚举
// ============================================================
enum class KeyModifier : uint32_t
{
    None      = 0,
    Shift     = 1 << 0,
    Control   = 1 << 1,
    Alt       = 1 << 2,
    Super     = 1 << 3,      // Windows/Command key
    CapsLock  = 1 << 4,
    NumLock   = 1 << 5
};

inline KeyModifier operator|(KeyModifier lhs, KeyModifier rhs)
{
    return static_cast<KeyModifier>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline KeyModifier operator&(KeyModifier lhs, KeyModifier rhs)
{
    return static_cast<KeyModifier>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline bool hasModifier(KeyModifier flags, KeyModifier modifier)
{
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(modifier)) != 0;
}

// ============================================================
// 鼠标按钮枚举
// ============================================================
enum class MouseButton : uint32_t
{
    Button1        = 0,
    Button2        = 1,
    Button3        = 2,
    Button4        = 3,
    Button5        = 4,
    Button6        = 5,
    Button7        = 6,
    Button8        = 7,
    Left           = Button1,
    Right          = Button2,
    Middle         = Button3,
    Last           = Button8,
    None           = 0xFFFFFFFF
};

// ============================================================
// 游戏手柄按钮枚举
// ============================================================
enum class GamepadButton : uint32_t
{
    A              = 0,
    B              = 1,
    X              = 2,
    Y              = 3,
    LeftBumper     = 4,
    RightBumper    = 5,
    Back           = 6,
    Start          = 7,
    Guide          = 8,
    LeftThumb      = 9,
    RightThumb     = 10,
    DPadUp         = 11,
    DPadRight      = 12,
    DPadDown       = 13,
    DPadLeft       = 14,
    Last           = DPadLeft,
    Cross          = A,
    Circle         = B,
    Square         = X,
    Triangle       = Y
};

// ============================================================
// 游戏手柄轴枚举
// ============================================================
enum class GamepadAxis : uint32_t
{
    LeftX          = 0,
    LeftY          = 1,
    RightX         = 2,
    RightY         = 3,
    LeftTrigger    = 4,
    RightTrigger   = 5,
    Last           = RightTrigger
};

// ============================================================
// 事件基类
// ============================================================
class Event
{
public:
    virtual ~Event() = default;

    virtual EventType getEventType() const = 0;
    virtual const char* getName() const = 0;
    virtual int getCategoryFlags() const = 0;

    inline bool isInCategory(EventCategory category)
    {
        return getCategoryFlags() & static_cast<int>(category);
    }

    inline void setHandled(bool handled) { m_handled = handled; }
    inline bool isHandled() const { return m_handled; }

    virtual std::string toString() const
    {
        return getName();
    }

protected:
    bool m_handled = false;
};

// ============================================================
// 窗口事件
// ============================================================
class WindowCloseEvent : public Event
{
public:
    WindowCloseEvent() = default;

    EventType getEventType() const override { return EventType::WindowClose; }
    const char* getName() const override { return "WindowCloseEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Window) |
               static_cast<int>(EventCategory::Application);
    }
};

class WindowResizeEvent : public Event
{
public:
    WindowResizeEvent(int width, int height)
        : m_width(width), m_height(height) {}

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    EventType getEventType() const override { return EventType::WindowResize; }
    const char* getName() const override { return "WindowResizeEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Window) |
               static_cast<int>(EventCategory::Application);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": " + std::to_string(m_width) + ", " + std::to_string(m_height);
    }

private:
    int m_width;
    int m_height;
};

class WindowMoveEvent : public Event
{
public:
    WindowMoveEvent(int x, int y)
        : m_x(x), m_y(y) {}

    int getX() const { return m_x; }
    int getY() const { return m_y; }

    EventType getEventType() const override { return EventType::WindowMove; }
    const char* getName() const override { return "WindowMoveEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Window);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": (" + std::to_string(m_x) + ", " + std::to_string(m_y) + ")";
    }

private:
    int m_x;
    int m_y;
};

class WindowFocusEvent : public Event
{
public:
    WindowFocusEvent(bool focused) : m_focused(focused) {}
    bool isFocused() const { return m_focused; }

    EventType getEventType() const override
    {
        return m_focused ? EventType::WindowFocus : EventType::WindowLostFocus;
    }
    const char* getName() const override
    {
        return m_focused ? "WindowFocusEvent" : "WindowLostFocusEvent";
    }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Window);
    }

private:
    bool m_focused;
};

class WindowRefreshEvent : public Event
{
public:
    WindowRefreshEvent() = default;

    EventType getEventType() const override { return EventType::WindowRefresh; }
    const char* getName() const override { return "WindowRefreshEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Window) |
               static_cast<int>(EventCategory::Application);
    }
};

// ============================================================
// 键盘事件
// ============================================================
class KeyEvent : public Event
{
public:
    KeyEvent(int keyCode) : m_keyCode(keyCode) {}
    int getKeyCode() const { return m_keyCode; }

    EventType getEventType() const override { return EventType::None; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Keyboard) |
               static_cast<int>(EventCategory::Input);
    }

protected:
    int m_keyCode;
};

class KeyPressedEvent : public KeyEvent
{
public:
    KeyPressedEvent(int keyCode, int repeatCount = 0)
        : KeyEvent(keyCode), m_repeatCount(repeatCount) {}

    int getRepeatCount() const { return m_repeatCount; }

    EventType getEventType() const override { return EventType::KeyPressed; }
    const char* getName() const override { return "KeyPressedEvent"; }

    std::string toString() const override
    {
        return std::string(getName()) + ": " + std::to_string(m_keyCode) +
               " (repeat: " + std::to_string(m_repeatCount) + ")";
    }

private:
    int m_repeatCount;
};

class KeyReleasedEvent : public KeyEvent
{
public:
    explicit KeyReleasedEvent(int keyCode) : KeyEvent(keyCode) {}

    EventType getEventType() const override { return EventType::KeyReleased; }
    const char* getName() const override { return "KeyReleasedEvent"; }

    std::string toString() const override
    {
        return std::string(getName()) + ": " + std::to_string(m_keyCode);
    }
};

class KeyTypedEvent : public Event
{
public:
    KeyTypedEvent(int keyCode) : m_keyCode(keyCode) {}
    int getKeyCode() const { return m_keyCode; }

    EventType getEventType() const override { return EventType::KeyTyped; }
    const char* getName() const override { return "KeyTypedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Keyboard) |
               static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": " + std::to_string(m_keyCode);
    }

private:
    int m_keyCode;
};

// ============================================================
// 鼠标事件
// ============================================================
class MouseMovedEvent : public Event
{
public:
    MouseMovedEvent(float x, float y) : m_mouseX(x), m_mouseY(y) {}

    float getX() const { return m_mouseX; }
    float getY() const { return m_mouseY; }

    EventType getEventType() const override { return EventType::MouseMoved; }
    const char* getName() const override { return "MouseMovedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Mouse) |
               static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": (" + std::to_string(m_mouseX) + ", " + std::to_string(m_mouseY) + ")";
    }

private:
    float m_mouseX;
    float m_mouseY;
};

class MouseScrolledEvent : public Event
{
public:
    MouseScrolledEvent(float xOffset, float yOffset)
        : m_xOffset(xOffset), m_yOffset(yOffset) {}

    float getXOffset() const { return m_xOffset; }
    float getYOffset() const { return m_yOffset; }

    EventType getEventType() const override { return EventType::MouseScrolled; }
    const char* getName() const override { return "MouseScrolledEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Mouse) |
               static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": (" + std::to_string(m_xOffset) + ", " + std::to_string(m_yOffset) + ")";
    }

private:
    float m_xOffset;
    float m_yOffset;
};

class MouseButtonEvent : public Event
{
public:
    explicit MouseButtonEvent(int button) : m_button(button) {}
    int getMouseButton() const { return m_button; }

    EventType getEventType() const override { return EventType::None; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::MouseButton) |
               static_cast<int>(EventCategory::Mouse) |
               static_cast<int>(EventCategory::Input);
    }

protected:
    int m_button;
};

class MouseButtonPressedEvent : public MouseButtonEvent
{
public:
    explicit MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {}

    EventType getEventType() const override { return EventType::MouseButtonPressed; }
    const char* getName() const override { return "MouseButtonPressedEvent"; }

    std::string toString() const override
    {
        return std::string(getName()) + ": " + std::to_string(m_button);
    }
};

class MouseButtonReleasedEvent : public MouseButtonEvent
{
public:
    explicit MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {}

    EventType getEventType() const override { return EventType::MouseButtonReleased; }
    const char* getName() const override { return "MouseButtonReleasedEvent"; }

    std::string toString() const override
    {
        return std::string(getName()) + ": " + std::to_string(m_button);
    }
};

// ============================================================
// 应用事件
// ============================================================
class AppUpdateEvent : public Event
{
public:
    AppUpdateEvent(float deltaTime)
        : m_deltaTime(deltaTime) {}

    float getDeltaTime() const { return m_deltaTime; }

    EventType getEventType() const override { return EventType::AppUpdate; }
    const char* getName() const override { return "AppUpdateEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Application);
    }

private:
    float m_deltaTime;
};

class AppRenderEvent : public Event
{
public:
    AppRenderEvent() = default;

    EventType getEventType() const override { return EventType::AppRender; }
    const char* getName() const override { return "AppRenderEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Application);
    }
};

class AppTickEvent : public Event
{
public:
    AppTickEvent() = default;

    EventType getEventType() const override { return EventType::AppTick; }
    const char* getName() const override { return "AppTickEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Application);
    }
};

// ============================================================
// 扩展窗口事件
// ============================================================
class WindowMinimizedEvent : public Event
{
public:
    WindowMinimizedEvent() = default;

    EventType getEventType() const override { return EventType::WindowMinimized; }
    const char* getName() const override { return "WindowMinimizedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Window);
    }
};

class WindowMaximizedEvent : public Event
{
public:
    WindowMaximizedEvent() = default;

    EventType getEventType() const override { return EventType::WindowMaximized; }
    const char* getName() const override { return "WindowMaximizedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Window);
    }
};

class WindowRestoredEvent : public Event
{
public:
    WindowRestoredEvent() = default;

    EventType getEventType() const override { return EventType::WindowRestored; }
    const char* getName() const override { return "WindowRestoredEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Window);
    }
};

class WindowScaleChangeEvent : public Event
{
public:
    WindowScaleChangeEvent(float xScale, float yScale)
        : m_xScale(xScale), m_yScale(yScale) {}

    float getXScale() const { return m_xScale; }
    float getYScale() const { return m_yScale; }

    EventType getEventType() const override { return EventType::WindowScaleChange; }
    const char* getName() const override { return "WindowScaleChangeEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Window);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": (" + std::to_string(m_xScale) + ", " + std::to_string(m_yScale) + ")";
    }

private:
    float m_xScale;
    float m_yScale;
};

// ============================================================
// 扩展键盘事件
// ============================================================
class KeyDownEvent : public KeyEvent
{
public:
    KeyDownEvent(int keyCode, int repeatCount = 0)
        : KeyEvent(keyCode), m_repeatCount(repeatCount) {}

    int getRepeatCount() const { return m_repeatCount; }

    EventType getEventType() const override { return EventType::KeyDown; }
    const char* getName() const override { return "KeyDownEvent"; }

    std::string toString() const override
    {
        return std::string(getName()) + ": " + std::to_string(m_keyCode) +
               " (repeat: " + std::to_string(m_repeatCount) + ")";
    }

private:
    int m_repeatCount;
};

class KeyUpEvent : public KeyEvent
{
public:
    explicit KeyUpEvent(int keyCode) : KeyEvent(keyCode) {}

    EventType getEventType() const override { return EventType::KeyUp; }
    const char* getName() const override { return "KeyUpEvent"; }

    std::string toString() const override
    {
        return std::string(getName()) + ": " + std::to_string(m_keyCode);
    }
};

class KeyRepeatEvent : public KeyEvent
{
public:
    KeyRepeatEvent(int keyCode, int repeatCount = 1)
        : KeyEvent(keyCode), m_repeatCount(repeatCount) {}

    int getRepeatCount() const { return m_repeatCount; }

    EventType getEventType() const override { return EventType::KeyRepeat; }
    const char* getName() const override { return "KeyRepeatEvent"; }

    std::string toString() const override
    {
        return std::string(getName()) + ": " + std::to_string(m_keyCode) +
               " (count: " + std::to_string(m_repeatCount) + ")";
    }

private:
    int m_repeatCount;
};

class CharInputEvent : public Event
{
public:
    CharInputEvent(unsigned int codepoint) : m_codepoint(codepoint) {}

    unsigned int getCodepoint() const { return m_codepoint; }
    char getChar() const { return static_cast<char>(m_codepoint); }

    EventType getEventType() const override { return EventType::CharInput; }
    const char* getName() const override { return "CharInputEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Keyboard) |
               static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": '" + std::string(1, getChar()) + "'";
    }

private:
    unsigned int m_codepoint;
};

class ModifierPressedEvent : public Event
{
public:
    ModifierPressedEvent(KeyModifier modifiers, bool pressed)
        : m_modifiers(modifiers), m_pressed(pressed) {}

    KeyModifier getModifiers() const { return m_modifiers; }
    bool isPressed() const { return m_pressed; }
    bool hasShift() const { return hasModifier(m_modifiers, KeyModifier::Shift); }
    bool hasControl() const { return hasModifier(m_modifiers, KeyModifier::Control); }
    bool hasAlt() const { return hasModifier(m_modifiers, KeyModifier::Alt); }
    bool hasSuper() const { return hasModifier(m_modifiers, KeyModifier::Super); }

    EventType getEventType() const override { return EventType::ModifierPressed; }
    const char* getName() const override { return "ModifierPressedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Keyboard) |
               static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": " + std::to_string(static_cast<uint32_t>(m_modifiers)) +
               " (pressed: " + std::to_string(m_pressed) + ")";
    }

private:
    KeyModifier m_modifiers;
    bool m_pressed;
};

// ============================================================
// 扩展鼠标事件
// ============================================================
class MouseEnteredEvent : public Event
{
public:
    MouseEnteredEvent() = default;

    EventType getEventType() const override { return EventType::MouseEntered; }
    const char* getName() const override { return "MouseEnteredEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Mouse) |
               static_cast<int>(EventCategory::Input);
    }
};

class MouseLeftEvent : public Event
{
public:
    MouseLeftEvent() = default;

    EventType getEventType() const override { return EventType::MouseLeft; }
    const char* getName() const override { return "MouseLeftEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Mouse) |
               static_cast<int>(EventCategory::Input);
    }
};

class MouseDraggedEvent : public Event
{
public:
    MouseDraggedEvent(float x, float y, int button)
        : m_mouseX(x), m_mouseY(y), m_button(button) {}

    float getX() const { return m_mouseX; }
    float getY() const { return m_mouseY; }
    int getButton() const { return m_button; }

    EventType getEventType() const override { return EventType::MouseDragged; }
    const char* getName() const override { return "MouseDraggedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Mouse) |
               static_cast<int>(EventCategory::MouseButton) |
               static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": (" + std::to_string(m_mouseX) +
               ", " + std::to_string(m_mouseY) + ") button: " + std::to_string(m_button);
    }

private:
    float m_mouseX;
    float m_mouseY;
    int m_button;
};

// ============================================================
// 触摸事件
// ============================================================
class TouchBeganEvent : public Event
{
public:
    TouchBeganEvent(int fingerId, float x, float y)
        : m_fingerId(fingerId), m_x(x), m_y(y) {}

    int getFingerId() const { return m_fingerId; }
    float getX() const { return m_x; }
    float getY() const { return m_y; }

    EventType getEventType() const override { return EventType::TouchBegan; }
    const char* getName() const override { return "TouchBeganEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": finger=" + std::to_string(m_fingerId) +
               " (" + std::to_string(m_x) + ", " + std::to_string(m_y) + ")";
    }

private:
    int m_fingerId;
    float m_x;
    float m_y;
};

class TouchEndedEvent : public Event
{
public:
    TouchEndedEvent(int fingerId, float x, float y)
        : m_fingerId(fingerId), m_x(x), m_y(y) {}

    int getFingerId() const { return m_fingerId; }
    float getX() const { return m_x; }
    float getY() const { return m_y; }

    EventType getEventType() const override { return EventType::TouchEnded; }
    const char* getName() const override { return "TouchEndedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": finger=" + std::to_string(m_fingerId) +
               " (" + std::to_string(m_x) + ", " + std::to_string(m_y) + ")";
    }

private:
    int m_fingerId;
    float m_x;
    float m_y;
};

class TouchMovedEvent : public Event
{
public:
    TouchMovedEvent(int fingerId, float x, float y, float dx, float dy)
        : m_fingerId(fingerId), m_x(x), m_y(y), m_dx(dx), m_dy(dy) {}

    int getFingerId() const { return m_fingerId; }
    float getX() const { return m_x; }
    float getY() const { return m_y; }
    float getDeltaX() const { return m_dx; }
    float getDeltaY() const { return m_dy; }

    EventType getEventType() const override { return EventType::TouchMoved; }
    const char* getName() const override { return "TouchMovedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": finger=" + std::to_string(m_fingerId) +
               " (" + std::to_string(m_x) + ", " + std::to_string(m_y) + ")";
    }

private:
    int m_fingerId;
    float m_x;
    float m_y;
    float m_dx;
    float m_dy;
};

class TouchCancelledEvent : public Event
{
public:
    TouchCancelledEvent(int fingerId) : m_fingerId(fingerId) {}

    int getFingerId() const { return m_fingerId; }

    EventType getEventType() const override { return EventType::TouchCancelled; }
    const char* getName() const override { return "TouchCancelledEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": finger=" + std::to_string(m_fingerId);
    }

private:
    int m_fingerId;
};

// ============================================================
// 游戏手柄事件
// ============================================================
class GamepadConnectedEvent : public Event
{
public:
    GamepadConnectedEvent(int gamepadId) : m_gamepadId(gamepadId) {}

    int getGamepadId() const { return m_gamepadId; }

    EventType getEventType() const override { return EventType::GamepadConnected; }
    const char* getName() const override { return "GamepadConnectedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": id=" + std::to_string(m_gamepadId);
    }

private:
    int m_gamepadId;
};

class GamepadDisconnectedEvent : public Event
{
public:
    GamepadDisconnectedEvent(int gamepadId) : m_gamepadId(gamepadId) {}

    int getGamepadId() const { return m_gamepadId; }

    EventType getEventType() const override { return EventType::GamepadDisconnected; }
    const char* getName() const override { return "GamepadDisconnectedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": id=" + std::to_string(m_gamepadId);
    }

private:
    int m_gamepadId;
};

class GamepadButtonPressedEvent : public Event
{
public:
    GamepadButtonPressedEvent(int gamepadId, uint32_t button)
        : m_gamepadId(gamepadId), m_button(button) {}

    int getGamepadId() const { return m_gamepadId; }
    uint32_t getButton() const { return m_button; }

    EventType getEventType() const override { return EventType::GamepadButtonPressed; }
    const char* getName() const override { return "GamepadButtonPressedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": gamepad=" + std::to_string(m_gamepadId) +
               " button=" + std::to_string(m_button);
    }

private:
    int m_gamepadId;
    uint32_t m_button;
};

class GamepadButtonReleasedEvent : public Event
{
public:
    GamepadButtonReleasedEvent(int gamepadId, uint32_t button)
        : m_gamepadId(gamepadId), m_button(button) {}

    int getGamepadId() const { return m_gamepadId; }
    uint32_t getButton() const { return m_button; }

    EventType getEventType() const override { return EventType::GamepadButtonReleased; }
    const char* getName() const override { return "GamepadButtonReleasedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": gamepad=" + std::to_string(m_gamepadId) +
               " button=" + std::to_string(m_button);
    }

private:
    int m_gamepadId;
    uint32_t m_button;
};

class GamepadAxisMovedEvent : public Event
{
public:
    GamepadAxisMovedEvent(int gamepadId, uint32_t axis, float value)
        : m_gamepadId(gamepadId), m_axis(axis), m_value(value) {}

    int getGamepadId() const { return m_gamepadId; }
    uint32_t getAxis() const { return m_axis; }
    float getValue() const { return m_value; }

    EventType getEventType() const override { return EventType::GamepadAxisMoved; }
    const char* getName() const override { return "GamepadAxisMovedEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Input);
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": gamepad=" + std::to_string(m_gamepadId) +
               " axis=" + std::to_string(m_axis) + " value=" + std::to_string(m_value);
    }

private:
    int m_gamepadId;
    uint32_t m_axis;
    float m_value;
};

// ============================================================
// 文件拖放事件
// ============================================================
class FileDropEvent : public Event
{
public:
    FileDropEvent(int count, const char** paths)
        : m_count(count)
    {
        for (int i = 0; i < count && i < 16; ++i)
        {
            m_paths.push_back(paths[i]);
        }
    }

    int getCount() const { return m_count; }
    const std::string& getPath(int index) const { return m_paths[index]; }
    const std::vector<std::string>& getPaths() const { return m_paths; }

    EventType getEventType() const override { return EventType::FileDrop; }
    const char* getName() const override { return "FileDropEvent"; }
    int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::Input);
    }

private:
    int m_count;
    std::vector<std::string> m_paths;
};

// ============================================================
// 自定义事件
// ============================================================
class CustomEvent : public Event
{
public:
    CustomEvent(uint32_t type, uint64_t data1 = 0, uint64_t data2 = 0)
        : m_customType(type), m_data1(data1), m_data2(data2) {}

    uint32_t getCustomType() const { return m_customType; }
    uint64_t getData1() const { return m_data1; }
    uint64_t getData2() const { return m_data2; }

    EventType getEventType() const override { return EventType::Custom; }
    const char* getName() const override { return "CustomEvent"; }
    int getCategoryFlags() const override
    {
        return 0; // 自定义事件不属于任何预定义类别
    }

    std::string toString() const override
    {
        return std::string(getName()) + ": type=" + std::to_string(m_customType) +
               " data=(" + std::to_string(m_data1) + ", " + std::to_string(m_data2) + ")";
    }

private:
    uint32_t m_customType;
    uint64_t m_data1;
    uint64_t m_data2;
};

// ============================================================
// 事件监听器基类
// ============================================================
class IEventListener
{
public:
    virtual ~IEventListener() = default;
    virtual bool onEvent(const Event& event) = 0;
};

// ============================================================
// 事件分发器
// ============================================================
class EventDispatcher
{
public:
    using EventCallback = std::function<bool(Event&)>;
    using ListenerID = uint32_t;

    EventDispatcher() = default;
    ~EventDispatcher() = default;

    // 禁用拷贝
    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    // 启用移动
    EventDispatcher(EventDispatcher&&) = default;
    EventDispatcher& operator=(EventDispatcher&&) = default;

    // 注册事件监听器（模板版本）
    template<typename T>
    ListenerID addEventListener(std::function<bool(T&)> callback)
    {
        static_assert(std::is_base_of<Event, T>::value,
                      "T must be derived from Event");

        auto wrapper = [callback](Event& event) -> bool
        {
            return callback(static_cast<T&>(event));
        };

        return addEventListenerInternal(
            std::type_index(typeid(T)),
            std::move(wrapper)
        );
    }

    // 注册通用事件监听器
    ListenerID addEventListener(EventType type, EventCallback callback)
    {
        ListenerID id = m_nextListenerID++;
        m_eventListeners[static_cast<uint32_t>(type)].emplace_back(id, std::move(callback));
        return id;
    }

    // 注册按类别监听器
    ListenerID addCategoryListener(EventCategory category, EventCallback callback)
    {
        ListenerID id = m_nextListenerID++;
        m_categoryListeners[static_cast<int>(category)].emplace_back(id, std::move(callback));
        return id;
    }

    // 移除监听器
    bool removeEventListener(ListenerID id)
    {
        bool removed = false;

        for (auto& [type, listeners] : m_eventListeners)
        {
            auto it = std::remove_if(listeners.begin(), listeners.end(),
                [id](const auto& pair) { return pair.first == id; });
            if (it != listeners.end())
            {
                listeners.erase(it, listeners.end());
                removed = true;
            }
        }

        for (auto& [category, listeners] : m_categoryListeners)
        {
            auto it = std::remove_if(listeners.begin(), listeners.end(),
                [id](const auto& pair) { return pair.first == id; });
            if (it != listeners.end())
            {
                listeners.erase(it, listeners.end());
                removed = true;
            }
        }

        return removed;
    }

    // 分发事件
    void dispatch(Event& event)
    {
        uint32_t typeIndex = static_cast<uint32_t>(event.getEventType());

        // 派发到类型特定的监听器
        auto typeIt = m_eventListeners.find(typeIndex);
        if (typeIt != m_eventListeners.end())
        {
            for (auto& [id, callback] : typeIt->second)
            {
                if (event.isHandled())
                    break;
                event.setHandled(callback(event));
            }
        }

        // 派发到类别监听器
        int categoryFlags = event.getCategoryFlags();
        for (auto& [category, listeners] : m_categoryListeners)
        {
            if (categoryFlags & category)
            {
                for (auto& [id, callback] : listeners)
                {
                    if (event.isHandled())
                        break;
                    event.setHandled(callback(event));
                }
            }
        }
    }

    // 清空所有监听器
    void clear()
    {
        m_eventListeners.clear();
        m_categoryListeners.clear();
    }

    // 获取监听器数量
    size_t getListenerCount() const
    {
        size_t count = 0;
        for (const auto& [type, listeners] : m_eventListeners)
        {
            count += listeners.size();
        }
        for (const auto& [category, listeners] : m_categoryListeners)
        {
            count += listeners.size();
        }
        return count;
    }

private:
    ListenerID addEventListenerInternal(std::type_index type, EventCallback callback)
    {
        ListenerID id = m_nextListenerID++;
        m_eventListeners[static_cast<uint32_t>(type.hash_code())].emplace_back(id, std::move(callback));
        return id;
    }

private:
    std::map<uint32_t, std::vector<std::pair<ListenerID, EventCallback>>> m_eventListeners;
    std::map<int, std::vector<std::pair<ListenerID, EventCallback>>> m_categoryListeners;
    ListenerID m_nextListenerID = 0;
};

// ============================================================
// 事件总线（全局事件系统）
// ============================================================
class EventBus
{
public:
    static EventBus& getInstance()
    {
        static EventBus instance;
        return instance;
    }

    // 禁用拷贝和移动
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    void dispatch(Event& event)
    {
        m_dispatcher.dispatch(event);
    }

    template<typename T>
    EventDispatcher::ListenerID subscribe(std::function<bool(T&)> callback)
    {
        return m_dispatcher.addEventListener(callback);
    }

    EventDispatcher::ListenerID subscribe(EventType type, EventDispatcher::EventCallback callback)
    {
        return m_dispatcher.addEventListener(type, std::move(callback));
    }

    EventDispatcher::ListenerID subscribe(EventCategory category, EventDispatcher::EventCallback callback)
    {
        return m_dispatcher.addCategoryListener(category, std::move(callback));
    }

    bool unsubscribe(EventDispatcher::ListenerID id)
    {
        return m_dispatcher.removeEventListener(id);
    }

    void clear()
    {
        m_dispatcher.clear();
    }

private:
    EventBus() = default;

    EventDispatcher m_dispatcher;
};

// ============================================================
// 便捷宏定义
// ============================================================
#define EVENT_CLASS_TYPE(type) \
    EventType getEventType() const override { return EventType::##type; } \
    const char* getName() const override { return #type "Event"; }

#define EVENT_CLASS_CATEGORY(...) \
    int getCategoryFlags() const override \
    { \
        int flags = 0; \
        ((flags |= static_cast<int>(__VA_ARGS__)), ...); \
        return flags; \
    }

} // namespace yaln

#endif // YALN_EVENT_H
