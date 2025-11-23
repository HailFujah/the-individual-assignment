#include <graphics.h>      // EasyX图形库，用于窗口创建和图形绘制
#include <Windows.h>       // Windows API，用于窗口管理、消息处理等系统功能
#include <stdio.h>         // 标准输入输出函数
#include <time.h>          // 时间相关函数，用于随机数种子和计时
#include <math.h>          // 数学函数，用于宠物移动物理计算
#include <algorithm>       // 标准算法库，用于数值范围限制等
#include <thread>          // C++11线程库，用于多线程处理
#include <mutex>           // 互斥锁，用于多线程同步
#include <atomic>          // 原子变量，用于线程间安全通信
#include <vector>          // 动态数组容器，用于存储聊天历史
#include <string>          // 字符串处理
#include <sstream>         // 字符串流，用于格式化输出
#include <wininet.h>       // Windows网络库，用于HTTP请求
#include <iostream>        // 输入输出流
#include <fstream>         // 文件流，用于日志记录
#include <locale>          // 本地化支持
#include <codecvt>         // 字符编码转换
#include <chrono>          // 时间库，用于高精度计时

// 链接所需的Windows库
#pragma comment(lib, "wininet.lib")    // 网络请求库
#pragma comment(lib, "gdi32.lib")      // 图形设备接口库
#pragma comment(lib, "comctl32.lib")   // 通用控件库
#pragma comment(lib, "ws2_32.lib")     // Windows套接字库

// GCC编译器的链接指示
#ifdef __GNUC__
#pragma comment(linker, "-lwininet")
#pragma comment(linker, "-lgdi32")
#pragma comment(linker, "-lcomctl32")
#pragma comment(linker, "-lws2_32")
#endif

// AI服务配置
const std::string API_KEY = "sk-zifiaxxcbgtzxfjvarqxomldnonlthzesppnxpzuascndeka";  // API密钥
const std::string API_URL = "https://api.siliconflow.cn/v1/chat/completions";      // API地址
const std::string MODEL_NAME = "Qwen/QwQ-32B";                                     // 使用的AI模型名称

// 前置声明
class DesktopPet;

/**
 * 聊天消息结构体
 * 用于存储聊天历史中的单条消息
 */
struct ChatMessage {
    std::string fullText;     // 完整消息文本
    std::string currentText;  // 当前显示的文本（用于打字机效果）
    bool isUser;              // 是否为用户消息
    bool isComplete;          // 消息是否完全显示
    int charIndex;            // 当前显示到的字符索引
};

/**
 * 聊天窗口消息处理函数
 * 处理聊天窗口的各种消息事件
 */
LRESULT CALLBACK ChatWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * 聊天线程数据结构
 * 用于向聊天窗口线程传递必要的数据
 */
struct ChatThreadData {
    DesktopPet* pet;         // 桌面宠物实例指针
    HWND parentHwnd;         // 父窗口句柄
    HINSTANCE hInstance;     // 应用实例句柄
};

/**
 * 保存API响应到调试文件
 * @param content 要保存的响应内容
 */
void saveResponseToFile(const std::string& content) {
    std::ofstream file("api_response_debug.txt", std::ios::app);
    if (file.is_open()) {
        time_t now = time(0);
        char* dt = ctime(&now);
        file << "[" << dt << "] Response:\n" << content << "\n\n" << std::string(50, '-') << "\n";
        file.close();
    }
}

/**
 * 发送HTTP POST请求
 * @param url 请求的URL
 * @param data 请求的数据
 * @param contentType 内容类型
 * @param authHeader 认证头信息
 * @return 服务器响应内容
 */
std::string httpPostRequest(const std::string& url, const std::string& data, const std::string& contentType, const std::string& authHeader = "") {
    // 初始化Internet会话
    HINTERNET hInternet = InternetOpenA("DesktopPet/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        std::cout << "InternetOpen failed. Error: " << GetLastError() << std::endl;
        return "";
    }
    
    std::string result;
    // 建立与服务器的连接
    HINTERNET hConnect = InternetConnectA(hInternet, "api.siliconflow.cn", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        std::cout << "InternetConnect failed. Error: " << GetLastError() << std::endl;
        InternetCloseHandle(hInternet);
        return "";
    }
    
    // 指定可接受的内容类型
    LPCSTR rgpszAcceptTypes[] = {"application/json", NULL};
    // 打开HTTP请求
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", "/v1/chat/completions", NULL, NULL, rgpszAcceptTypes, INTERNET_FLAG_SECURE, 0);
    if (!hRequest) {
        std::cout << "HttpOpenRequest failed. Error: " << GetLastError() << std::endl;
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }
    
    // 构建请求头
    std::string headers = "Content-Type: " + contentType + "\r\n";
    if (!authHeader.empty()) {
        headers += authHeader + "\r\n";
    }
    
    // 发送HTTP请求
    if (!HttpSendRequestA(hRequest, headers.c_str(), headers.length(), (LPVOID)data.c_str(), data.length())) {
        std::cout << "HttpSendRequest failed. Error: " << GetLastError() << std::endl;
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }
    
    // 获取HTTP状态码
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL);
    std::cout << "HTTP Status Code: " << statusCode << std::endl;
    
    // 读取响应内容
    char buffer[4096];
    DWORD bytesRead;
    std::string fullResponse;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        fullResponse += buffer;
    }
    
    // 检查HTTP状态码是否为成功
    if (statusCode != 200) {
        std::cout << "API Error Details: " << fullResponse << std::endl;
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }
    
    result = fullResponse;
    saveResponseToFile(result);  // 保存响应到调试文件
    
    // 清理资源
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    return result;
}

/**
 * 桌面宠物主类
 * 实现宠物的显示、移动、交互和聊天功能
 */
class DesktopPet
{
private:
    /**
     * 宠物状态枚举
     * 定义宠物可能的行为状态
     */
    enum PetState { 
        relax,    // 休息状态
        move,     // 移动状态
        interact, // 交互状态
        sit       // 坐立状态
    };
    
    // 每个状态的动画帧数
    const int StateFrameCount[4] = {40, 46, 40, 320};
    
    /**
     * 宠物数据结构
     * 存储宠物的位置、速度、状态等信息
     */
    struct Pet {
        float x, y;             // 位置坐标
        float vx, vy;           // 速度分量
        float vmax;             // 最大速度
        float ax, ay;           // 加速度分量
        float a;                // 最大加速度
        bool facingRight;       // 是否面向右方
        PetState state;         // 当前状态
        int stateTimeLeft;      // 状态剩余时间
    };
    
    HWND hwnd;                  // 窗口句柄
    int windowWidth, windowHeight; // 窗口宽度和高度
    POINT pt_src;               // 图像源位置
    SIZE size_wnd;              // 窗口大小
    BLENDFUNCTION blend;        // 混合函数，用于透明窗口
    
    Pet pet;                    // 宠物实例
    bool isSitting;             // 是否处于坐立状态
    bool dragging;              // 是否正在拖动窗口
    POINT lastMousePos;         // 上一次鼠标位置
    
    const int FrameDelay;       // 帧延迟（毫秒），控制动画速度
    int stateFrameIndex[4];     // 每个状态的当前动画帧索引
    PetState lastState;         // 上一个状态
    
    IMAGE* petImages[2][4][320]; // 宠物图像数组 [方向][状态][帧]
    
    bool showMenu;              // 是否显示右键菜单
    RECT menuRect;              // 菜单矩形区域
    const RECT sitButtonRel;    // 坐立按钮相对位置
    const RECT chatButtonRel;   // 聊天按钮相对位置
    
    HHOOK hMouseHook;           // 鼠标钩子句柄
    static DesktopPet* instance; // 单例实例指针
    
    std::atomic<HWND> hChatWnd; // 聊天窗口句柄（原子变量，线程安全）
    std::atomic<bool> showChatWindow; // 是否显示聊天窗口
    std::thread chatThread;     // 聊天窗口线程
    std::mutex chatMutex;       // 聊天相关互斥锁
    std::atomic<bool> chatThreadRunning; // 聊天线程是否运行中
    RECT lastPetRect;           // 上一次宠物窗口位置
    
    std::vector<ChatMessage> chatHistory; // 聊天历史记录
    std::mutex historyMutex;    // 聊天历史互斥锁
    std::atomic<bool> isTyping; // 是否正在打字（显示打字机效果）
    std::atomic<bool> isProcessingMessage; // 是否正在处理消息
    std::mutex interactMutex;   // 交互互斥锁，确保单次触发
    std::atomic<uint64_t> lastTriggerTime; // 最后触发交互的时间
    
    // 聊天线程入口函数
    static void ChatThreadEntry(ChatThreadData* data);
    // 发送聊天窗口位置更新
    void SendChatWindowPosUpdate();
    // 获取宠物窗口矩形区域
    RECT GetPetWindowRect() const;
    // 初始化窗口
    void InitializeWindow();
    // 加载资源（图像等）
    void LoadResources();
    // 更新宠物状态
    void UpdatePetState();
    // 处理输入
    void HandleInput();
    // 绘制界面
    void Draw();
    // 鼠标钩子处理函数
    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    // 内部鼠标钩子处理
    LRESULT HandleMouseHookInternal(int nCode, WPARAM wParam, LPARAM lParam);
    // 创建聊天窗口
    void CreateChatWindow();
    // 隐藏聊天窗口
    void HideChatWindow();
    // AI请求线程
    static void AIRequestThread(DesktopPet* pet, const std::string& message);
    // 调用OpenAI兼容API
    std::string CallOpenAIAPI(const std::string& prompt);
    // 添加聊天消息
    void AddChatMessage(const std::string& text, bool isUser);
    // 打字机效果
    static void TypeWriterEffect(DesktopPet* pet, size_t messageIndex);
    // 转义JSON特殊字符
    std::string escapeJson(const std::string& s);
    // 解析OpenAI响应
    std::string parseOpenAIResponse(const std::string& json);
    // UTF8转宽字符
    std::wstring utf8ToWide(const std::string& utf8);
    // 截断内容
    std::string truncateContent(const std::string& content, int maxLen = 4000);
    // 移除空字符
    std::string removeNullCharacters(const std::string& s);
    // 获取当前毫秒数
    uint64_t GetCurrentTimeMs();
    
public:
    // 检查是否正在打字
    bool IsTyping() const { return isTyping; }
    // 构造函数
    DesktopPet(int width = 300, int height = 300);
    // 析构函数
    ~DesktopPet();
    // 运行主循环
    void Run();
    // 获取窗口句柄
    HWND GetHwnd() const { return hwnd; }
    // 切换聊天窗口显示状态
    void ToggleChatWindow();
    // 设置聊天窗口状态
    static void SetChatWindowState(bool state);
    // 检查点是否在聊天窗口内
    bool IsPointInChatWindow(POINT pt) const;
    // 发送消息给AI
    void SendMessageToAI(const std::string& message);
    // 更新聊天显示
    void UpdateChatDisplay();
};

// 初始化静态成员
DesktopPet* DesktopPet::instance = nullptr;

/**
 * 获取当前时间的毫秒数
 * 用于高精度计时和事件间隔控制
 * @return 当前时间的毫秒数
 */
uint64_t DesktopPet::GetCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

/**
 * 移除字符串中的空字符
 * 防止空字符导致的显示问题
 * @param s 输入字符串
 * @return 移除空字符后的字符串
 */
std::string DesktopPet::removeNullCharacters(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (c != '\0') {
            result += c;
        }
    }
    return result;
}

/**
 * 将UTF8字符串转换为宽字符串
 * 用于Windows API的字符串处理
 * @param utf8 UTF8编码的字符串
 * @return 宽字符串
 */
std::wstring DesktopPet::utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    // 计算所需缓冲区大小
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(len, L'\0');
    // 执行转换
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], len);
    return wide;
}

/**
 * 转义JSON中的特殊字符
 * 确保生成的JSON格式正确
 * @param s 输入字符串
 * @return 转义后的字符串
 */
std::string DesktopPet::escapeJson(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                // 转义非打印字符
                if (c < 32 || c > 126) {
                    char buf[10];
                    sprintf(buf, "\\u%04X", (unsigned int)(unsigned char)c);
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

/**
 * 截断内容到指定长度
 * 防止消息过长导致的问题
 * @param content 输入内容
 * @param maxLen 最大长度
 * @return 截断后的内容
 */
std::string DesktopPet::truncateContent(const std::string& content, int maxLen) {
    if (content.size() <= maxLen) return content;
    return content.substr(0, maxLen) + "...";
}

/**
 * 解析OpenAI格式的API响应
 * 提取出AI的回复内容
 * @param json API返回的JSON字符串
 * @return 提取出的回复内容
 */
std::string DesktopPet::parseOpenAIResponse(const std::string& json) {
    std::cout << "Parsing JSON: " << json << std::endl;
    
    if (json.empty()) {
        std::cout << "Error: Empty JSON response" << std::endl;
        return "[Error: Empty response from server]";
    }
    
    // 查找choices数组
    size_t choicesPos = json.find("\"choices\":[");
    if (choicesPos == std::string::npos) {
        std::cout << "Error: Could not find choices array" << std::endl;
        return "[Error: No choices in response]";
    }
    
    // 查找message对象
    size_t messagePos = json.find("\"message\":{", choicesPos);
    if (messagePos == std::string::npos) {
        std::cout << "Error: Could not find message object" << std::endl;
        return "[Error: No message in response]";
    }
    
    // 查找content字段
    size_t contentPos = json.find("\"content\"", messagePos);
    if (contentPos == std::string::npos) {
        std::cout << "Error: Could not find content field" << std::endl;
        return "[Error: No content in message]";
    }
    
    // 查找冒号
    size_t colonPos = json.find(":", contentPos);
    if (colonPos == std::string::npos) {
        std::cout << "Error: Could not find colon after content" << std::endl;
        return "[Error: Invalid content format]";
    }
    
    // 找到值的起始位置
    size_t valueStart = colonPos + 1;
    while (valueStart < json.size() && (json[valueStart] == ' ' || json[valueStart] == '\t' || json[valueStart] == '\n' || json[valueStart] == '\r')) {
        valueStart++;
    }
    
    // 检查值是否以引号开头
    if (valueStart >= json.size() || json[valueStart] != '"') {
        std::cout << "Error: Content value does not start with quote" << std::endl;
        return "[Error: Invalid content value format]";
    }
    valueStart++;
    
    // 找到值的结束位置（考虑转义字符）
    size_t valueEnd = valueStart;
    bool inEscape = false;
    while (valueEnd < json.size()) {
        if (json[valueEnd] == '"' && !inEscape) {
            break;
        }
        inEscape = (json[valueEnd] == '\\' && !inEscape);
        valueEnd++;
    }
    
    // 检查是否找到结束引号
    if (valueEnd >= json.size()) {
        std::cout << "Error: Could not find end of content value" << std::endl;
        return "[Error: Incomplete response content]";
    }
    
    // 提取并处理内容
    std::string response = json.substr(valueStart, valueEnd - valueStart);
    std::cout << "Raw extracted content: [" << response << "]" << std::endl;
    
    // 替换转义字符
    size_t pos;
    while ((pos = response.find("\\n")) != std::string::npos) {
        response.replace(pos, 2, "\n");
    }
    while ((pos = response.find("\\\"")) != std::string::npos) {
        response.replace(pos, 2, "\"");
    }
    while ((pos = response.find("\\\\\\")) != std::string::npos) {
        response.replace(pos, 2, "\\");
    }
    
    // 去除开头的空白字符
    size_t start = 0;
    while (start < response.size() && (response[start] == '\n' || response[start] == '\r' || response[start] == ' ')) {
        start++;
    }
    response = response.substr(start);
    
    // 处理空响应
    if (response.empty()) {
        std::cout << "Warning: Processed response is empty" << std::endl;
        return "[Amiya seems to have nothing to say right now...]";
    }
    
    std::cout << "Processed response: [" << response << "]" << std::endl;
    return response;
}

/**
 * 聊天窗口消息处理函数
 * 处理聊天窗口的各种事件
 */
LRESULT CALLBACK ChatWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // 创建聊天历史显示区域
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | 
                ES_AUTOVSCROLL | ES_READONLY | WS_TABSTOP,
                5, 5, 290, 110, hwnd, (HMENU)1, 
                ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            
            // 创建消息输入框
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP | ES_NOHIDESEL,
                5, 120, 210, 50, hwnd, (HMENU)2, 
                ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            
            // 创建发送按钮
            CreateWindowW(L"BUTTON", L"Send", 
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                220, 120, 75, 50, hwnd, (HMENU)3, 
                ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            
            // 创建并设置字体
            HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            
            // 为控件设置字体
            SendMessageW(GetDlgItem(hwnd, 1), WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(GetDlgItem(hwnd, 2), WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(GetDlgItem(hwnd, 3), WM_SETFONT, (WPARAM)hFont, TRUE);
            
            // 设置焦点到输入框
            SetFocus(GetDlgItem(hwnd, 2));
            return 0;
        }
        case WM_COMMAND: {
            // 检测发送按钮点击
            bool isSendButton = (LOWORD(wParam) == 3 && HIWORD(wParam) == BN_CLICKED);
            
            // 获取宠物实例
            DesktopPet* pet = (DesktopPet*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (isSendButton && pet && !pet->IsTyping()) {
                SendMessageW(hwnd, WM_KILLFOCUS, 0, 0);
                HWND hInput = GetDlgItem(hwnd, 2);
                int len = GetWindowTextLengthW(hInput);
                if (len <= 0) break;
                
                // 获取输入框内容
                wchar_t* text = new wchar_t[len + 1];
                GetWindowTextW(hInput, text, len + 1);
                
                if (pet) {
                    // 转换为UTF8并发送给AI
                    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
                    std::string utf8Text(utf8Len, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, text, -1, &utf8Text[0], utf8Len, nullptr, nullptr);
                    pet->SendMessageToAI(utf8Text);
                }
                
                // 清空输入框
                SetWindowTextW(hInput, L"");
                delete[] text;
                SetFocus(hInput);
                return 0;
            }
            return 0;
        }
        // 处理回车键发送消息
        case WM_KEYDOWN: {
            if (wParam == VK_RETURN && !(GetKeyState(VK_SHIFT) & 0x8000)) {
                HWND hInput = GetFocus();
                if (hInput && GetDlgCtrlID(hInput) == 2) { // 确认焦点在输入框
                    DesktopPet* pet = (DesktopPet*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
                    if (pet && !pet->IsTyping()) {
                        int len = GetWindowTextLengthW(hInput);
                        if (len > 0) {
                            // 获取并处理输入内容
                            wchar_t* text = new wchar_t[len + 1];
                            GetWindowTextW(hInput, text, len + 1);
                            
                            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
                            std::string utf8Text(utf8Len, '\0');
                            WideCharToMultiByte(CP_UTF8, 0, text, -1, &utf8Text[0], utf8Len, nullptr, nullptr);
                            pet->SendMessageToAI(utf8Text);
                            
                            // 清空输入框
                            SetWindowTextW(hInput, L"");
                            delete[] text;
                        }
                        return 0; // 吞噬回车键，避免换行
                    }
                }
            }
            break;
        }
        case WM_CLOSE:
            // 隐藏窗口而不是销毁
            ShowWindow(hwnd, SW_HIDE);
            DesktopPet::SetChatWindowState(false);
            return 0;
        case WM_USER + 100: {
            // 更新聊天窗口位置
            RECT* petRect = (RECT*)lParam;
            if (petRect) {
                // 根据宠物位置计算聊天窗口位置
                int chatX = (petRect->left < GetSystemMetrics(SM_CXSCREEN)/2) 
                    ? petRect->right 
                    : petRect->left - 300;
                int chatY = petRect->top;
                
                // 确保聊天窗口在屏幕内
                if (chatX < 0) chatX = 0;
                if (chatX + 300 > GetSystemMetrics(SM_CXSCREEN))
                    chatX = GetSystemMetrics(SM_CXSCREEN) - 300;
                if (chatY < 0) chatY = 0;
                
                // 移动窗口
                SetWindowPos(hwnd, HWND_TOP, chatX, chatY, 0, 0, SWP_NOSIZE);
                delete petRect;
            }
            return 0;
        }
        case WM_USER + 101: {
            // 更新聊天显示
            DesktopPet* pet = (DesktopPet*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (pet) {
                pet->UpdateChatDisplay();
            }
            return 0;
        }
        case WM_SETFOCUS: {
            // 设置焦点到输入框
            SetFocus(GetDlgItem(hwnd, 2));
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}
    
/**
 * 聊天线程入口函数
 * 负责创建和管理聊天窗口
 */
void DesktopPet::ChatThreadEntry(ChatThreadData* data) {
    DesktopPet* pet = data->pet;
    HINSTANCE hInstance = data->hInstance;
    delete data; // 释放传递的数据
    
    // 注册窗口类
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = ChatWindowProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ChatWindowClass_Safe";
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Chat window class registration failed", L"Error", MB_ICONERROR);
        pet->chatThreadRunning = false;
        return;
    }
    
    // 创建聊天窗口
    HWND hChatWnd = CreateWindowExW(
        0,
        L"ChatWindowClass_Safe",
        L"Chat with Amiya",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, // 不可调整大小，不可最大化
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 180,
        pet->GetHwnd(), NULL, hInstance, NULL
    );
    
    if (!hChatWnd) {
        MessageBoxW(NULL, L"Chat window creation failed", L"Error", MB_ICONERROR);
        pet->chatThreadRunning = false;
        return;
    }
    
    // 存储宠物实例指针
    SetWindowLongPtrW(hChatWnd, GWLP_USERDATA, (LONG_PTR)pet);
    pet->hChatWnd = hChatWnd;
    
    // 显示窗口并设置焦点
    ShowWindow(hChatWnd, SW_SHOW);
    SetFocus(hChatWnd);
    pet->showChatWindow = true;
    pet->SendChatWindowPosUpdate();
    
    // 消息循环
    MSG msg;
    while (pet->chatThreadRunning) {
        if (GetMessage(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else break;
    }
    
    // 清理资源
    DestroyWindow(hChatWnd);
    UnregisterClassW(L"ChatWindowClass_Safe", hInstance);
    pet->hChatWnd = NULL;
    pet->chatThreadRunning = false;
}

/**
 * 发送聊天窗口位置更新
 * 确保聊天窗口始终位于宠物旁边
 */
void DesktopPet::SendChatWindowPosUpdate() {
    HWND hChat = hChatWnd;
    if (hChat && showChatWindow) {
        RECT currentPetRect = GetPetWindowRect();
        // 只有位置变化较大时才更新
        if (abs(currentPetRect.left - lastPetRect.left) > 10 ||
            abs(currentPetRect.top - lastPetRect.top) > 10) {
            RECT* petRect = new RECT(currentPetRect);
            PostMessageW(hChat, WM_USER + 100, 0, (LPARAM)petRect);
            lastPetRect = currentPetRect;
        }
    }
}

/**
 * 获取宠物窗口的矩形区域
 * @return 包含窗口位置和大小的RECT结构体
 */
RECT DesktopPet::GetPetWindowRect() const {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    return rect;
}

/**
 * 设置聊天窗口状态
 * @param state 新状态（显示/隐藏）
 */
void DesktopPet::SetChatWindowState(bool state) {
    if (instance) instance->showChatWindow = state;
}

/**
 * 检查点是否在聊天窗口内
 * @param pt 要检查的点
 * @return 如果在窗口内返回true，否则返回false
 */
bool DesktopPet::IsPointInChatWindow(POINT pt) const {
    HWND hChat = hChatWnd;
    if (!hChat) return false;
    RECT chatRect;
    GetWindowRect(hChat, &chatRect);
    return PtInRect(&chatRect, pt);
}

/**
 * 添加聊天消息到历史记录
 * @param text 消息文本
 * @param isUser 是否为用户消息
 */
void DesktopPet::AddChatMessage(const std::string& text, bool isUser) {
    std::string cleanedText = removeNullCharacters(text);
    std::lock_guard<std::mutex> lock(historyMutex); // 确保线程安全
    
    ChatMessage msg;
    msg.fullText = cleanedText;
    msg.isUser = isUser;
    
    if (isUser) {
        // 用户消息立即完全显示
        msg.currentText = cleanedText;
        msg.isComplete = true;
        msg.charIndex = cleanedText.length();
        chatHistory.push_back(msg);
        // 限制历史记录数量
        if (chatHistory.size() > 50) chatHistory.erase(chatHistory.begin());
        // 更新显示
        HWND hChat = hChatWnd;
        if (hChat) PostMessageW(hChat, WM_USER + 101, 0, 0);
    } else {
        // AI消息使用打字机效果
        msg.currentText = "";
        msg.isComplete = false;
        msg.charIndex = 0;
        chatHistory.push_back(msg);
        // 限制历史记录数量
        if (chatHistory.size() > 50) chatHistory.erase(chatHistory.begin());
        
        // 开始打字机效果
        isTyping = true;
        std::thread(TypeWriterEffect, this, chatHistory.size() - 1).detach();
    }
}

/**
 * 打字机效果实现
 * 逐字显示AI回复，增强交互体验
 * @param pet 桌面宠物实例
 * @param messageIndex 消息在历史记录中的索引
 */
void DesktopPet::TypeWriterEffect(DesktopPet* pet, size_t messageIndex) {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(pet->historyMutex);
            
            // 检查消息是否仍然存在
            if (messageIndex >= pet->chatHistory.size()) {
                pet->isTyping = false;
                return;
            }
            
            ChatMessage& msg = pet->chatHistory[messageIndex];
            
            // 检查是否已经显示完所有字符
            if (msg.charIndex >= msg.fullText.length()) {
                msg.isComplete = true;
                pet->isTyping = false;
                // 更新显示并闪烁窗口提醒用户
                HWND hChat = pet->hChatWnd;
                if (hChat) {
                    PostMessageW(hChat, WM_USER + 101, 0, 0);
                    FlashWindow(hChat, FALSE);
                }
                return;
            }
            
            // 显示下一个字符
            msg.charIndex++;
            msg.currentText = msg.fullText.substr(0, msg.charIndex);
            
            // 更新显示
            HWND hChat = pet->hChatWnd;
            if (hChat) {
                PostMessageW(hChat, WM_USER + 101, 0, 0);
            }
        }
        
        // 根据字符类型调整延迟，增强打字机效果
        char currentChar = pet->chatHistory[messageIndex].fullText[pet->chatHistory[messageIndex].charIndex - 1];
        if ((currentChar & 0x80) != 0) { // 中文字符
            Sleep(80);
        } else if (currentChar == ' ' || currentChar == '\n' || currentChar == '\r') { // 空格或换行
            Sleep(150);
        } else { // 英文字符
            Sleep(50);
        }
    }
}

/**
 * 更新聊天显示内容
 * 将聊天历史记录显示在聊天窗口中
 */
void DesktopPet::UpdateChatDisplay() {
    HWND hChat = hChatWnd;
    if (!hChat) return;
    HWND hHistory = GetDlgItem(hChat, 1);
    if (!hHistory) {
        std::cout << "Error: Could not get history control" << std::endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(historyMutex);
    std::stringstream ss;
    // 构建聊天历史文本
    for (const auto& msg : chatHistory) {
        if (msg.isUser) {
            ss << "You: " << msg.currentText << "\r\n";
        } else {
            ss << "Amiya: " << msg.currentText << (msg.isComplete ? "" : "...") << "\r\n";
        }
        ss << "\r\n";
    }
    
    // 更新显示
    std::string chatText = ss.str();
    std::wstring wChatText = utf8ToWide(chatText);
    SetWindowTextW(hHistory, wChatText.c_str());
    // 滚动到最新内容
    SendMessageW(hHistory, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(hHistory, EM_SCROLLCARET, 0, 0);
}

/**
 * 发送消息给AI
 * @param message 要发送的消息
 */
void DesktopPet::SendMessageToAI(const std::string& message) {
    // 双重防护：互斥锁 + 时间间隔检查（3秒内不重复触发）
    std::lock_guard<std::mutex> lock(interactMutex);
    uint64_t now = GetCurrentTimeMs();
    if (now - lastTriggerTime < 3000) {
        return;
    }
    lastTriggerTime = now;
    
    isProcessingMessage = true;
    
    // 清理消息中的换行符
    std::string cleanedMessage = message;
    size_t pos;
    while ((pos = cleanedMessage.find("\r\n")) != std::string::npos) {
        cleanedMessage.replace(pos, 2, " ");
    }
    while ((pos = cleanedMessage.find("\n")) != std::string::npos) {
        cleanedMessage.replace(pos, 1, " ");
    }
    
    // 添加到聊天历史并发送请求
    AddChatMessage(cleanedMessage, true);
    std::thread(AIRequestThread, this, cleanedMessage).detach();
    
    // 强制切换到interact状态并重置动画
    pet.state = interact;
    pet.vx = pet.vy = 0;
    pet.stateTimeLeft = 120;  // 3秒动画时长
    stateFrameIndex[interact] = 0;
    lastState = interact;
    
    // 动画结束后解锁
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        isProcessingMessage = false;
    }).detach();
}

/**
 * AI请求线程函数
 * 在后台线程中处理AI请求，避免阻塞UI
 * @param pet 桌面宠物实例
 * @param message 要发送的消息
 */
void DesktopPet::AIRequestThread(DesktopPet* pet, const std::string& message) {
    std::string response = pet->CallOpenAIAPI(message);
    pet->AddChatMessage(response, false);
}

/**
 * 调用OpenAI兼容API
 * @param prompt 用户输入的提示
 * @return AI的回复
 */
std::string DesktopPet::CallOpenAIAPI(const std::string& prompt) {
    try {
        // 系统提示，定义AI的角色和行为
        std::string systemPrompt = "You are Amiya from Arknights. Stay in character and reply as her. Respond naturally to the Doctor's messages. Just output one or two sentences to make your answers brief. Do not start your responses with blank lines or extra whitespace.";
        // 背景对话，提供上下文
        std::string backgroundPrompt = "Welcome home, Doctor! Hi, Amiya. How are you? I'm doing great, Doctor! My team just recently helped defend Lungmen from a Reunion attack. This fight was one of the biggest we've had yet, but thanks to Rhodes Island we were able to hold the line. Good. I'm glad to hear that. By the way I just bought snacks for the operators. For lunch time. Oh, are those the snacks that Ptilopsis is making now? How do the rest of the operators like them? So true. Luckily I got it before it ran out. You can leave that to Wafarin, Amiya. By the way, did Kal'tsit call me while I was away? Thank you for reminding me to talk to Wafarin about that. As for Kal'tsit, I'm afraid I have bad news. Her calls, and communications with the others, are being blocked or intercepted. As you know, she is very private about her past, but from what her messages said, she was investigating one of her old contacts who may have known something about her past. Since then, it's been radio silence. Same thing, Amiya. Well, I do have an idea of where she might be. There's a large cave just south of Rhodes Island. It's been sealed up for a long while. According to Kal'tsit's information, it was an old Reunion base. She thinks that, maybe, whoever is blocking her communications is located there. If we could locate and eliminate this new threat, Kal'tsit might finally be able to come back! Okay. I'll see him soon with another operator. And I ask that you stay here guarding the base, Amiya. While I go to the location. I feel like I should follow you there, but you're right. The base is still full of Reunion troops. The new threat is still there but unseen. It's possible that they could attack the base during the operation. This base is important, Doctor. It is a central point in our logistics network.";
        
        // 构建JSON请求
        std::string json = "{\"model\": \"" + MODEL_NAME + "\", \"messages\": [";
        json += "{\"role\": \"system\", \"content\": \"" + escapeJson(systemPrompt) + "\"},";
        json += "{\"role\": \"assistant\", \"content\": \"" + escapeJson(truncateContent(backgroundPrompt)) + "\"},";
        json += "{\"role\": \"user\", \"content\": \"" + escapeJson(prompt) + "\"}";
        json += "]}";
        
        // 发送请求
        std::string authHeader = "Authorization: Bearer " + API_KEY;
        std::string response = httpPostRequest(API_URL, json, "application/json", authHeader);
        
        // 处理响应
        if (response.empty()) return "[Error: No response from AI service]";
        return parseOpenAIResponse(response);
    } catch (...) {
        return "[Error: An error occurred while processing your request]";
    }
}

/**
 * 构造函数
 * 初始化宠物的各种属性和资源
 * @param width 窗口宽度
 * @param height 窗口高度
 */
DesktopPet::DesktopPet(int width, int height) 
    : windowWidth(width), windowHeight(height), isSitting(false), dragging(false),
      showMenu(false), hMouseHook(NULL), isTyping(false), FrameDelay(1000 / 40),
      sitButtonRel{10, 10, 110, 40}, chatButtonRel{10, 50, 110, 80} {
    instance = this;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    // 初始化帧索引
    for (int i = 0; i < 4; i++) {
        stateFrameIndex[i] = 0;
    }
    
    // 初始化宠物属性
    pet = {
        (float)(screenW / 2 - windowWidth / 2),  // 初始X位置（屏幕中央）
        (float)(screenH / 2 - windowHeight / 2), // 初始Y位置（屏幕中央）
        0, 0, 3.0f, 0, 0, 0.3f,                 // 速度、加速度相关
        true, relax, 100                         // 初始面向右方，休息状态
    };
    
    // 初始化窗口和资源
    InitializeWindow();
    LoadResources();
    // 设置鼠标钩子，用于全局鼠标事件捕获
    hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(NULL), 0);
    lastPetRect = GetPetWindowRect();
}

/**
 * 析构函数
 * 清理资源，释放内存
 */
DesktopPet::~DesktopPet() {
    // 停止聊天线程
    chatThreadRunning = false;
    if (hChatWnd) PostMessageW(hChatWnd, WM_QUIT, 0, 0);
    if (chatThread.joinable()) chatThread.join();
    
    // 移除鼠标钩子
    if (hMouseHook) UnhookWindowsHookEx(hMouseHook);
    
    // 释放图像资源
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < StateFrameCount[j]; k++)
                delete petImages[i][j][k];
    
    // 关闭图形窗口
    closegraph();
}

/**
 * 初始化窗口
 * 设置窗口样式、透明度等属性
 */
void DesktopPet::InitializeWindow() {
    // 创建图形窗口
    hwnd = initgraph(windowWidth, windowHeight, EW_DBLCLKS | EW_SHOWCONSOLE);
    // 设置窗口样式：无边框、可拖动
    LONG l_WinStyle = GetWindowLongW(hwnd, GWL_STYLE);
    SetWindowLongW(hwnd, GWL_STYLE, (l_WinStyle | WS_POPUP) & ~WS_CAPTION & ~WS_THICKFRAME & ~WS_BORDER);
    // 设置窗口为分层窗口，支持透明效果
    SetWindowLongW(hwnd, GWL_EXSTYLE, GetWindowLongW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    
    // 初始化透明混合参数
    pt_src = {0, 0};
    size_wnd = {windowWidth, windowHeight};
    
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
}

/**
 * 加载资源
 * 加载宠物的各种状态和方向的图像
 */
void DesktopPet::LoadResources() {
    // 图像路径数组 [方向][状态]
    const char* path[2][4] = {
        {"assets\\relax\\relax1\\build_char_002_amiya_R_25_",
         "assets\\move\\move1\\build_char_002_amiya_M_25_",
         "assets\\interact\\interact1\\build_char_002_amiya_I_25_",
         "assets\\sit\\sit1\\build_char_002_amiya_S_25_"},
        
        {"assets\\relax\\relax2\\build_char_002_amiya_R_w_25_",
         "assets\\move\\move2\\build_char_002_amiya_M_w_25_",
         "assets\\interact\\interact2\\build_char_002_amiya_I_w_25_",
         "assets\\sit\\sit2\\build_char_002_amiya_S_w_25_"}
    };
    
    // 加载所有图像
    char name[256];
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 4; j++)
        {
            int frameCount = StateFrameCount[j];
            for (int k = 0; k < frameCount; k++)
            {
                // 构建图像文件名
                sprintf(name, "%s%03d.png", path[i][j], k);
                petImages[i][j][k] = new IMAGE();
                loadimage(petImages[i][j][k], name, windowWidth, windowHeight);
            }
        }
}

/**
 * 创建聊天窗口
 * 如果聊天线程未运行，则启动线程创建窗口
 * 否则显示已存在的窗口
 */
void DesktopPet::CreateChatWindow() {
    if (!chatThreadRunning) {
        chatThreadRunning = true;
        ChatThreadData* data = new ChatThreadData{this, hwnd, GetModuleHandleW(NULL)};
        chatThread = std::thread(&DesktopPet::ChatThreadEntry, data);
        chatThread.detach();
    } else {
        HWND hChat = hChatWnd;
        if (hChat) {
            ShowWindow(hChat, SW_SHOW);
            SetForegroundWindow(hChat);
            SetFocus(hChat);
            showChatWindow = true;
            SendChatWindowPosUpdate();
        }
    }
}

/**
 * 隐藏聊天窗口
 */
void DesktopPet::HideChatWindow() {
    HWND hChat = hChatWnd;
    if (hChat) {
        ShowWindow(hChat, SW_HIDE);
        showChatWindow = false;
    }
}

/**
 * 切换聊天窗口的显示状态
 * 如果显示则隐藏，反之亦然
 */
void DesktopPet::ToggleChatWindow() {
    if (showChatWindow) HideChatWindow();
    else CreateChatWindow();
}

/**
 * 更新宠物状态
 * 根据当前状态更新宠物的位置、速度等属性
 * 处理状态转换逻辑
 */
void DesktopPet::UpdatePetState() {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    // 根据当前状态更新行为
    switch (pet.state) {
        case interact:
            // 交互状态：播放动画，不允许被其他逻辑打断
            pet.stateTimeLeft--;
            if (pet.stateTimeLeft <= 0) {
                // 动画结束后根据情况切换状态
                pet.state = (showChatWindow || isSitting) ? sit : relax;
                pet.stateTimeLeft = rand() % 71 + 30;
            }
            break;
        case relax:
            // 休息状态：逐渐减速至停止
            pet.vx *= 0.9f;
            pet.vy *= 0.9f;
            if (fabsf(pet.vx) < 0.1f) pet.vx = 0;
            if (fabsf(pet.vy) < 0.1f) pet.vy = 0;
            
            // 状态计时
            pet.stateTimeLeft--;
            if (pet.stateTimeLeft <= 0) {
                // 休息结束，切换状态
                pet.state = isSitting ? sit : move;
                if (!isSitting) {
                    pet.stateTimeLeft = rand() % 151 + 50;
                    // 随机加速度
                    pet.ax = ((float)rand() / RAND_MAX) * 2 * pet.a - pet.a;
                    pet.ay = ((float)rand() / RAND_MAX) * 2 * pet.a - pet.a;
                    // 根据加速度方向设置朝向
                    pet.facingRight = pet.ax > 0;
                } else {
                    pet.stateTimeLeft = rand() % 71 + 30;
                }
            }
            break;
        case move:
            // 如果处于坐立状态，切换到坐立状态
            if (isSitting) {
                pet.state = sit;
                pet.stateTimeLeft = rand() % 71 + 30;
                break;
            }
            
            // 随机调整加速度
            if (rand() % 30 == 0) {
                pet.ax += ((float)rand() / RAND_MAX) * 0.1f - 0.05f;
                pet.ay += ((float)rand() / RAND_MAX) * 0.1f - 0.05f;
                // 限制加速度范围
                pet.ax = std::max(-pet.a, std::min(pet.a, pet.ax));
                pet.ay = std::max(-pet.a, std::min(pet.a, pet.ay));
                // 更新朝向
                pet.facingRight = pet.ax > 0;
            }
            
            // 更新速度
            pet.vx += pet.ax;
            pet.vy += pet.ay;
            // 限制速度范围
            pet.vx = std::max(-pet.vmax, std::min(pet.vmax, pet.vx));
            pet.vy = std::max(-pet.vmax, std::min(pet.vmax, pet.vy));
            
            // 更新位置
            pet.x += pet.vx;
            pet.y += pet.vy;
            
            // 边界碰撞检测与处理
            if (pet.x < 0) { 
                pet.x = 0; 
                pet.vx = -pet.vx; 
                pet.ax = -pet.ax; 
                pet.facingRight = true; 
            }
            if (pet.x + windowWidth > screenW) { 
                pet.x = screenW - windowWidth; 
                pet.vx = -pet.vx; 
                pet.ax = -pet.ax; 
                pet.facingRight = false; 
            }
            if (pet.y < 0) { 
                pet.y = 0; 
                pet.vy = -pet.vy; 
                pet.ay = -pet.ay; 
            }
            if (pet.y + windowHeight > screenH) { 
                pet.y = screenH - windowHeight; 
                pet.vy = -pet.vy; 
                pet.ay = -pet.ay; 
            }
            
            // 状态计时
            pet.stateTimeLeft--;
            if (pet.stateTimeLeft <= 0) {
                // 移动结束，切换到休息状态
                pet.state = relax;
                pet.stateTimeLeft = rand() % 71 + 30;
                // 设置减速加速度
                pet.ax = -pet.vx * 0.1f;
                pet.ay = -pet.vy * 0.1f;
            }
            break;
        case sit:
            // 坐立状态：静止不动
            pet.vx = 0;
            pet.vy = 0;
            pet.ax = 0;
            pet.ay = 0;
            
            // 状态计时
            pet.stateTimeLeft--;
            if (pet.stateTimeLeft <= 0) {
                if (isSitting) {
                    // 继续坐立
                    pet.stateTimeLeft = rand() % 71 + 30;
                } else {
                    // 切换到休息状态
                    pet.state = relax;
                    pet.stateTimeLeft = rand() % 71 + 30;
                }
            }
            break;
    }
    
    // 如果聊天窗口显示且宠物移动或被拖动，更新聊天窗口位置
    if (showChatWindow && (pet.state == move || dragging)) {
        SendChatWindowPosUpdate();
    }
    
    // 如果状态改变，重置帧索引
    if (pet.state != lastState) {
        stateFrameIndex[pet.state] = 0;
        if (pet.state == sit) {
            stateFrameIndex[sit] = 0;
        }
        lastState = pet.state;
    }
}

/**
 * 处理输入
 * 处理鼠标和键盘输入事件
 */
void DesktopPet::HandleInput() {
    ExMessage m;
    // 处理所有待处理消息
    while (peekmessage(&m, EM_MOUSE | EM_KEY)) {
        switch (m.message) {
            case WM_LBUTTONUP:
                // 鼠标左键释放，结束拖动
                dragging = false;
                ReleaseCapture();
                break;
            case WM_KEYDOWN:
                // 按下ESC键退出程序
                if (m.vkcode == VK_ESCAPE) {
                    closegraph();
                    exit(0);
                }
                break;
        }
    }
    
    // 处理窗口拖动
    if (dragging) {
        pet.state = relax;
        POINT curPos;
        GetCursorPos(&curPos);
        // 计算鼠标移动距离
        int dx = curPos.x - lastMousePos.x;
        int dy = curPos.y - lastMousePos.y;
        
        // 移动窗口
        RECT rect;
        GetWindowRect(hwnd, &rect);
        SetWindowPos(hwnd, 0, rect.left + dx, rect.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        
        // 更新鼠标位置和宠物位置
        lastMousePos = curPos;
        pet.x = (float)(rect.left + dx);
        pet.y = (float)(rect.top + dy);
    }
}

/**
 * 鼠标钩子处理函数
 * 全局鼠标事件的入口点
 */
LRESULT CALLBACK DesktopPet::MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (instance) return instance->HandleMouseHookInternal(nCode, wParam, lParam);
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

/**
 * 内部鼠标钩子处理
 * 处理与宠物交互的鼠标事件
 */
LRESULT DesktopPet::HandleMouseHookInternal(int nCode, WPARAM wParam, LPARAM lParam) {
    // 处理消息期间完全屏蔽鼠标事件
    if (isProcessingMessage) {
        return 1;  // 直接吞噬事件，不传递给下一个钩子
    }

    if (nCode >= 0) {
        MOUSEHOOKSTRUCT* p = (MOUSEHOOKSTRUCT*)lParam;
        POINT pt = p->pt;
        RECT wndRect;
        GetWindowRect(hwnd, &wndRect);
        bool inWindow = PtInRect(&wndRect, pt);
        
        if (IsPointInChatWindow(pt)) {
            return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
        }
        
        if (wParam == WM_LBUTTONDOWN && inWindow && !showMenu) {
            dragging = true;
            GetCursorPos(&lastMousePos);
            SetCapture(hwnd);
        }
        
        if (wParam == WM_LBUTTONUP && dragging) {
            dragging = false;
            ReleaseCapture();
        }
        
        if (wParam == WM_RBUTTONDOWN && inWindow && !showChatWindow) {
            // 右键触发interact也添加时间检查
            uint64_t now = GetCurrentTimeMs();
            if (now - lastTriggerTime > 1000 && pet.state != interact) {
                pet.state = interact;
                pet.vx = pet.vy = 0;
                pet.stateTimeLeft = 80;
                stateFrameIndex[interact] = 0;
                lastState = interact;
                lastTriggerTime = now;
            }
            
            GetCursorPos(&pt);
            menuRect.left = pt.x - wndRect.left;
            menuRect.top = pt.y - wndRect.top;
            
            if (menuRect.left + 120 > windowWidth) menuRect.left = windowWidth - 120;
            if (menuRect.top + 90 > windowHeight) menuRect.top = windowHeight - 90;
            
            menuRect.right = menuRect.left + 120;
            menuRect.bottom = menuRect.top + 90;
            showMenu = true;
        }
        
        if (wParam == WM_LBUTTONDOWN && showMenu) {
            POINT clickPt = pt;
            ScreenToClient(hwnd, &clickPt);
            
            RECT sitBtnAbs = {
                menuRect.left + sitButtonRel.left,
                menuRect.top + sitButtonRel.top,
                menuRect.left + sitButtonRel.right,
                menuRect.top + sitButtonRel.bottom
            };
            RECT chatBtnAbs = {
                menuRect.left + chatButtonRel.left,
                menuRect.top + chatButtonRel.top,
                menuRect.left + chatButtonRel.right,
                menuRect.top + chatButtonRel.bottom
            };
            
            if (PtInRect(&sitBtnAbs, clickPt)) {
                isSitting = !isSitting;
                if (isSitting) {
                    pet.state = sit;
                    pet.vx = pet.vy = 0;
                    stateFrameIndex[sit] = 0;
                    lastState = sit;
                } else {
                    pet.state = relax;
                    stateFrameIndex[relax] = 0;
                    lastState = relax;
                }
                showMenu = false;
            } 
            else if (PtInRect(&chatBtnAbs, clickPt)) {
                isSitting = true;
                pet.state = sit;
                stateFrameIndex[sit] = 0;
                lastState = sit;
                showMenu = false;
                ToggleChatWindow();
            } 
            else if (!inWindow) {
                showMenu = false;
            }
        }
    }
    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

/**
 * 绘制函数
 * 绘制宠物图像和菜单
 */
void DesktopPet::Draw() {
    int dir = pet.facingRight ? 0 : 1; // 根据朝向选择图像方向
    IMAGE tmp(windowWidth, windowHeight);
    SetWorkingImage(&tmp); // 设置临时绘图目标
    cleardevice(); // 清空绘图区域
    
    // 绘制当前帧图像
    putimage(0, 0, petImages[dir][pet.state][stateFrameIndex[pet.state]]);
    
    // 如果显示菜单，绘制菜单
    if (showMenu) {
        setfillcolor(RGB(240, 240, 240));
        solidrectangle(menuRect.left, menuRect.top, menuRect.right, menuRect.bottom);
        setlinecolor(RGB(180, 180, 180));
        rectangle(menuRect.left, menuRect.top, menuRect.right, menuRect.bottom);
        
        // 绘制坐立按钮
        RECT sitBtnAbs = {
            menuRect.left + sitButtonRel.left,
            menuRect.top + sitButtonRel.top,
            menuRect.left + sitButtonRel.right,
            menuRect.top + sitButtonRel.bottom
        };
        setfillcolor(isSitting ? RGB(255, 200, 200) : RGB(200, 255, 200));
        solidrectangle(sitBtnAbs.left, sitBtnAbs.top, sitBtnAbs.right, sitBtnAbs.bottom);
        setlinecolor(RGB(150, 150, 150));
        rectangle(sitBtnAbs.left, sitBtnAbs.top, sitBtnAbs.right, sitBtnAbs.bottom);
        settextcolor(BLACK);
        setbkmode(TRANSPARENT);
        outtextxy(sitBtnAbs.left + 10, sitBtnAbs.top + 5, isSitting ? _T("Sitting") : _T("Sit"));
        
        // 绘制聊天按钮
        RECT chatBtnAbs = {
            menuRect.left + chatButtonRel.left,
            menuRect.top + chatButtonRel.top,
            menuRect.left + chatButtonRel.right,
            menuRect.top + chatButtonRel.bottom
        };
        setfillcolor(RGB(200, 200, 255));
        solidrectangle(chatBtnAbs.left, chatBtnAbs.top, chatBtnAbs.right, chatBtnAbs.bottom);
        setlinecolor(RGB(150, 150, 150));
        rectangle(chatBtnAbs.left, chatBtnAbs.top, chatBtnAbs.right, chatBtnAbs.bottom);
        settextcolor(BLACK);
        setbkmode(TRANSPARENT);
        outtextxy(chatBtnAbs.left + 10, chatBtnAbs.top + 5, _T("Chat"));
    }
    
    // 将绘制结果更新到窗口
    SetWorkingImage(NULL);
    HDC hdcTmp = GetImageHDC(&tmp);
    UpdateLayeredWindow(hwnd, NULL, NULL, &size_wnd, hdcTmp, &pt_src, 0, &blend, ULW_ALPHA);
}

/**
 * 主循环
 * 处理输入、更新状态、绘制画面
 */
void DesktopPet::Run() {
    while (true) {
        int start = clock(); // 记录帧开始时间
        
        HandleInput();      // 处理输入
        UpdatePetState();   // 更新宠物状态
        
        // 更新窗口位置
        SetWindowPos(hwnd, 0, (int)pet.x, (int)pet.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        
        Draw();             // 绘制画面
        
        // 更新帧索引
        int currentState = pet.state;
        stateFrameIndex[currentState]++;
        if (stateFrameIndex[currentState] >= StateFrameCount[currentState]) {
            stateFrameIndex[currentState] = 0;
        }
        
        // 控制帧率
        int ft = clock() - start;
        if (FrameDelay - ft > 0) Sleep(FrameDelay - ft);
    }
}

// 声明DPI感知函数
extern "C" __declspec(dllimport) BOOL WINAPI SetProcessDPIAware();

/**
 * 主函数
 * 程序入口点
 */
int main() {
#ifdef _WIN32
    SetProcessDPIAware(); // 设置DPI感知，确保在高DPI屏幕上显示正常
#endif
    
    // 设置控制台编码为UTF8
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, "en_US.UTF-8");
    
    // 初始化随机数种子
    srand((unsigned int)time(NULL));
    // 创建并运行桌面宠物
    DesktopPet pet(300, 300);
    pet.Run();
    
    return 0;
}
