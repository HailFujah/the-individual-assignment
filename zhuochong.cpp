#include <graphics.h>
#include <Windows.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <sstream>
#include <wininet.h>
#include <iostream>
#include <fstream>
#include <locale>
#include <codecvt>
#include <chrono>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")

#ifdef __GNUC__
#pragma comment(linker, "-lwininet")
#pragma comment(linker, "-lgdi32")
#pragma comment(linker, "-lcomctl32")
#pragma comment(linker, "-lws2_32")
#endif

const std::string API_KEY = "sk-zifiaxxcbgtzxfjvarqxomldnonlthzesppnxpzuascndeka";
const std::string API_URL = "https://api.siliconflow.cn/v1/chat/completions";
const std::string MODEL_NAME = "Qwen/QwQ-32B";

class DesktopPet;

struct ChatMessage {
    std::string fullText;
    std::string currentText;
    bool isUser;
    bool isComplete;
    int charIndex;
};

LRESULT CALLBACK ChatWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct ChatThreadData {
    DesktopPet* pet;
    HWND parentHwnd;
    HINSTANCE hInstance;
};

void saveResponseToFile(const std::string& content) {
    std::ofstream file("api_response_debug.txt", std::ios::app);
    if (file.is_open()) {
        time_t now = time(0);
        char* dt = ctime(&now);
        file << "[" << dt << "] Response:\n" << content << "\n\n" << std::string(50, '-') << "\n";
        file.close();
    }
}

std::string httpPostRequest(const std::string& url, const std::string& data, const std::string& contentType, const std::string& authHeader = "") {
    HINTERNET hInternet = InternetOpenA("DesktopPet/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        std::cout << "InternetOpen failed. Error: " << GetLastError() << std::endl;
        return "";
    }
    
    std::string result;
    HINTERNET hConnect = InternetConnectA(hInternet, "api.siliconflow.cn", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        std::cout << "InternetConnect failed. Error: " << GetLastError() << std::endl;
        InternetCloseHandle(hInternet);
        return "";
    }
    
    LPCSTR rgpszAcceptTypes[] = {"application/json", NULL};
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", "/v1/chat/completions", NULL, NULL, rgpszAcceptTypes, INTERNET_FLAG_SECURE, 0);
    if (!hRequest) {
        std::cout << "HttpOpenRequest failed. Error: " << GetLastError() << std::endl;
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }
    
    std::string headers = "Content-Type: " + contentType + "\r\n";
    if (!authHeader.empty()) {
        headers += authHeader + "\r\n";
    }
    
    if (!HttpSendRequestA(hRequest, headers.c_str(), headers.length(), (LPVOID)data.c_str(), data.length())) {
        std::cout << "HttpSendRequest failed. Error: " << GetLastError() << std::endl;
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }
    
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL);
    std::cout << "HTTP Status Code: " << statusCode << std::endl;
    
    char buffer[4096];
    DWORD bytesRead;
    std::string fullResponse;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        fullResponse += buffer;
    }
    
    if (statusCode != 200) {
        std::cout << "API Error Details: " << fullResponse << std::endl;
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }
    
    result = fullResponse;
    saveResponseToFile(result);
    
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    return result;
}

class DesktopPet
{
private:
    enum PetState { 
        relax,
        move,
        interact,
        sit
    };
    
    const int StateFrameCount[4] = {40, 46, 40, 320};
    
    struct Pet {
        float x, y;
        float vx, vy;
        float vmax;
        float ax, ay;
        float a;
        bool facingRight;
        PetState state;
        int stateTimeLeft;
    };
    
    HWND hwnd;
    int windowWidth, windowHeight;
    POINT pt_src;
    SIZE size_wnd;
    BLENDFUNCTION blend;
    
    Pet pet;
    bool isSitting;
    bool dragging;
    POINT lastMousePos;
    
    const int FrameDelay = 1000 / 40;
    int stateFrameIndex[4];
    PetState lastState;
    
    IMAGE* petImages[2][4][320];
    
    bool showMenu;
    RECT menuRect;
    const RECT sitButtonRel;
    const RECT chatButtonRel;
    
    HHOOK hMouseHook;
    static DesktopPet* instance;
    
    std::atomic<HWND> hChatWnd{NULL};
    std::atomic<bool> showChatWindow{false};
    std::thread chatThread;
    std::mutex chatMutex;
    std::atomic<bool> chatThreadRunning {false};
    RECT lastPetRect;
    
    std::vector<ChatMessage> chatHistory;
    std::mutex historyMutex;
    std::atomic<bool> isTyping{false};
    std::atomic<bool> isProcessingMessage{false};
    std::mutex interactMutex;  // 新增互斥锁，确保单次触发
    std::atomic<uint64_t> lastTriggerTime{0};  // 记录最后触发时间
    
    static void ChatThreadEntry(ChatThreadData* data);
    void SendChatWindowPosUpdate();
    RECT GetPetWindowRect() const;
    void InitializeWindow();
    void LoadResources();
    void UpdatePetState();
    void HandleInput();
    void Draw();
    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMouseHookInternal(int nCode, WPARAM wParam, LPARAM lParam);
    void CreateChatWindow();
    void HideChatWindow();
    static void AIRequestThread(DesktopPet* pet, const std::string& message);
    std::string CallOpenAIAPI(const std::string& prompt);
    void AddChatMessage(const std::string& text, bool isUser);
    static void TypeWriterEffect(DesktopPet* pet, size_t messageIndex);
    std::string escapeJson(const std::string& s);
    std::string parseOpenAIResponse(const std::string& json);
    std::wstring utf8ToWide(const std::string& utf8);
    std::string truncateContent(const std::string& content, int maxLen = 4000);
    std::string removeNullCharacters(const std::string& s);
    uint64_t GetCurrentTimeMs();  // 新增：获取当前毫秒数
    
public:
    bool IsTyping() const { return isTyping; }
    DesktopPet(int width = 300, int height = 300);
    ~DesktopPet();
    void Run();
    HWND GetHwnd() const { return hwnd; }
    void ToggleChatWindow();
    static void SetChatWindowState(bool state);
    bool IsPointInChatWindow(POINT pt) const;
    void SendMessageToAI(const std::string& message);
    void UpdateChatDisplay();
};

DesktopPet* DesktopPet::instance = nullptr;

uint64_t DesktopPet::GetCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string DesktopPet::removeNullCharacters(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (c != '\0') {
            result += c;
        }
    }
    return result;
}

std::wstring DesktopPet::utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], len);
    return wide;
}

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

std::string DesktopPet::truncateContent(const std::string& content, int maxLen) {
    if (content.size() <= maxLen) return content;
    return content.substr(0, maxLen) + "...";
}

std::string DesktopPet::parseOpenAIResponse(const std::string& json) {
    std::cout << "Parsing JSON: " << json << std::endl;
    
    if (json.empty()) {
        std::cout << "Error: Empty JSON response" << std::endl;
        return "[Error: Empty response from server]";
    }
    
    size_t choicesPos = json.find("\"choices\":[");
    if (choicesPos == std::string::npos) {
        std::cout << "Error: Could not find choices array" << std::endl;
        return "[Error: No choices in response]";
    }
    
    size_t messagePos = json.find("\"message\":{", choicesPos);
    if (messagePos == std::string::npos) {
        std::cout << "Error: Could not find message object" << std::endl;
        return "[Error: No message in response]";
    }
    
    size_t contentPos = json.find("\"content\"", messagePos);
    if (contentPos == std::string::npos) {
        std::cout << "Error: Could not find content field" << std::endl;
        return "[Error: No content in message]";
    }
    
    size_t colonPos = json.find(":", contentPos);
    if (colonPos == std::string::npos) {
        std::cout << "Error: Could not find colon after content" << std::endl;
        return "[Error: Invalid content format]";
    }
    
    size_t valueStart = colonPos + 1;
    while (valueStart < json.size() && (json[valueStart] == ' ' || json[valueStart] == '\t' || json[valueStart] == '\n' || json[valueStart] == '\r')) {
        valueStart++;
    }
    
    if (valueStart >= json.size() || json[valueStart] != '"') {
        std::cout << "Error: Content value does not start with quote" << std::endl;
        return "[Error: Invalid content value format]";
    }
    valueStart++;
    
    size_t valueEnd = valueStart;
    bool inEscape = false;
    while (valueEnd < json.size()) {
        if (json[valueEnd] == '"' && !inEscape) {
            break;
        }
        inEscape = (json[valueEnd] == '\\' && !inEscape);
        valueEnd++;
    }
    
    if (valueEnd >= json.size()) {
        std::cout << "Error: Could not find end of content value" << std::endl;
        return "[Error: Incomplete response content]";
    }
    
    std::string response = json.substr(valueStart, valueEnd - valueStart);
    std::cout << "Raw extracted content: [" << response << "]" << std::endl;
    
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
    
    size_t start = 0;
    while (start < response.size() && (response[start] == '\n' || response[start] == '\r' || response[start] == ' ')) {
        start++;
    }
    response = response.substr(start);
    
    if (response.empty()) {
        std::cout << "Warning: Processed response is empty" << std::endl;
        return "[Amiya seems to have nothing to say right now...]";
    }
    
    std::cout << "Processed response: [" << response << "]" << std::endl;
    return response;
}

LRESULT CALLBACK ChatWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | 
                ES_AUTOVSCROLL | ES_READONLY | WS_TABSTOP,
                5, 5, 290, 110, hwnd, (HMENU)1, 
                ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP | ES_NOHIDESEL,
                5, 120, 210, 50, hwnd, (HMENU)2, 
                ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            
            CreateWindowW(L"BUTTON", L"Send", 
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                220, 120, 75, 50, hwnd, (HMENU)3, 
                ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            
            HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            
            SendMessageW(GetDlgItem(hwnd, 1), WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(GetDlgItem(hwnd, 2), WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(GetDlgItem(hwnd, 3), WM_SETFONT, (WPARAM)hFont, TRUE);
            
            SetFocus(GetDlgItem(hwnd, 2));
            return 0;
        }
        case WM_COMMAND: {
            // 精确检测按钮点击（使用BN_CLICKED）
            bool isSendButton = (LOWORD(wParam) == 3 && HIWORD(wParam) == BN_CLICKED);
            
            DesktopPet* pet = (DesktopPet*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (isSendButton && pet && !pet->IsTyping()) {
                SendMessageW(hwnd, WM_KILLFOCUS, 0, 0);
                HWND hInput = GetDlgItem(hwnd, 2);
                int len = GetWindowTextLengthW(hInput);
                if (len <= 0) break;
                
                wchar_t* text = new wchar_t[len + 1];
                GetWindowTextW(hInput, text, len + 1);
                
                if (pet) {
                    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
                    std::string utf8Text(utf8Len, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, text, -1, &utf8Text[0], utf8Len, nullptr, nullptr);
                    pet->SendMessageToAI(utf8Text);
                }
                
                SetWindowTextW(hInput, L"");
                delete[] text;
                SetFocus(hInput);
                return 0; // 处理后直接返回，阻止后续消息
            }
            return 0;
        }
        // 单独处理回车键（不依赖编辑框通知）
        case WM_KEYDOWN: {
            if (wParam == VK_RETURN && !(GetKeyState(VK_SHIFT) & 0x8000)) {
                HWND hInput = GetFocus();
                if (hInput && GetDlgCtrlID(hInput) == 2) { // 确认焦点在输入框
                    DesktopPet* pet = (DesktopPet*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
                    if (pet && !pet->IsTyping()) {
                        int len = GetWindowTextLengthW(hInput);
                        if (len > 0) {
                            wchar_t* text = new wchar_t[len + 1];
                            GetWindowTextW(hInput, text, len + 1);
                            
                            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
                            std::string utf8Text(utf8Len, '\0');
                            WideCharToMultiByte(CP_UTF8, 0, text, -1, &utf8Text[0], utf8Len, nullptr, nullptr);
                            pet->SendMessageToAI(utf8Text);
                            
                            SetWindowTextW(hInput, L"");
                            delete[] text;
                        }
                        return 0; // 吞噬回车键，避免重复触发
                    }
                }
            }
            break;
        }
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            DesktopPet::SetChatWindowState(false);
            return 0;
        case WM_USER + 100: {
            RECT* petRect = (RECT*)lParam;
            if (petRect) {
                int chatX = (petRect->left < GetSystemMetrics(SM_CXSCREEN)/2) 
                    ? petRect->right 
                    : petRect->left - 300;
                int chatY = petRect->top;
                
                if (chatX < 0) chatX = 0;
                if (chatX + 300 > GetSystemMetrics(SM_CXSCREEN))
                    chatX = GetSystemMetrics(SM_CXSCREEN) - 300;
                if (chatY < 0) chatY = 0;
                
                SetWindowPos(hwnd, HWND_TOP, chatX, chatY, 0, 0, SWP_NOSIZE);
                delete petRect;
            }
            return 0;
        }
        case WM_USER + 101: {
            DesktopPet* pet = (DesktopPet*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (pet) {
                pet->UpdateChatDisplay();
            }
            return 0;
        }
        case WM_SETFOCUS: {
            SetFocus(GetDlgItem(hwnd, 2));
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}
    
void DesktopPet::ChatThreadEntry(ChatThreadData* data) {
    DesktopPet* pet = data->pet;
    HINSTANCE hInstance = data->hInstance;
    delete data;
    
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
    
    HWND hChatWnd = CreateWindowExW(
        0,
        L"ChatWindowClass_Safe",
        L"Chat with Amiya",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 180,
        pet->GetHwnd(), NULL, hInstance, NULL
    );
    
    if (!hChatWnd) {
        MessageBoxW(NULL, L"Chat window creation failed", L"Error", MB_ICONERROR);
        pet->chatThreadRunning = false;
        return;
    }
    
    SetWindowLongPtrW(hChatWnd, GWLP_USERDATA, (LONG_PTR)pet);
    pet->hChatWnd = hChatWnd;
    
    ShowWindow(hChatWnd, SW_SHOW);
    SetFocus(hChatWnd);
    pet->showChatWindow = true;
    pet->SendChatWindowPosUpdate();
    
    MSG msg;
    while (pet->chatThreadRunning) {
        if (GetMessage(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else break;
    }
    
    DestroyWindow(hChatWnd);
    UnregisterClassW(L"ChatWindowClass_Safe", hInstance);
    pet->hChatWnd = NULL;
    pet->chatThreadRunning = false;
}

void DesktopPet::SendChatWindowPosUpdate() {
    HWND hChat = hChatWnd;
    if (hChat && showChatWindow) {
        RECT currentPetRect = GetPetWindowRect();
        if (abs(currentPetRect.left - lastPetRect.left) > 10 ||
            abs(currentPetRect.top - lastPetRect.top) > 10) {
            RECT* petRect = new RECT(currentPetRect);
            PostMessageW(hChat, WM_USER + 100, 0, (LPARAM)petRect);
            lastPetRect = currentPetRect;
        }
    }
}

RECT DesktopPet::GetPetWindowRect() const {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    return rect;
}

void DesktopPet::SetChatWindowState(bool state) {
    if (instance) instance->showChatWindow = state;
}

bool DesktopPet::IsPointInChatWindow(POINT pt) const {
    HWND hChat = hChatWnd;
    if (!hChat) return false;
    RECT chatRect;
    GetWindowRect(hChat, &chatRect);
    return PtInRect(&chatRect, pt);
}

void DesktopPet::AddChatMessage(const std::string& text, bool isUser) {
    std::string cleanedText = removeNullCharacters(text);
    std::lock_guard<std::mutex> lock(historyMutex);
    
    ChatMessage msg;
    msg.fullText = cleanedText;
    msg.isUser = isUser;
    
    if (isUser) {
        msg.currentText = cleanedText;
        msg.isComplete = true;
        msg.charIndex = cleanedText.length();
        chatHistory.push_back(msg);
        if (chatHistory.size() > 50) chatHistory.erase(chatHistory.begin());
        HWND hChat = hChatWnd;
        if (hChat) PostMessageW(hChat, WM_USER + 101, 0, 0);
    } else {
        msg.currentText = "";
        msg.isComplete = false;
        msg.charIndex = 0;
        chatHistory.push_back(msg);
        if (chatHistory.size() > 50) chatHistory.erase(chatHistory.begin());
        
        isTyping = true;
        std::thread(TypeWriterEffect, this, chatHistory.size() - 1).detach();
    }
}

void DesktopPet::TypeWriterEffect(DesktopPet* pet, size_t messageIndex) {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(pet->historyMutex);
            
            if (messageIndex >= pet->chatHistory.size()) {
                pet->isTyping = false;
                return;
            }
            
            ChatMessage& msg = pet->chatHistory[messageIndex];
            
            if (msg.charIndex >= msg.fullText.length()) {
                msg.isComplete = true;
                pet->isTyping = false;
                HWND hChat = pet->hChatWnd;
                if (hChat) {
                    PostMessageW(hChat, WM_USER + 101, 0, 0);
                    FlashWindow(hChat, FALSE);
                }
                return;
            }
            
            msg.charIndex++;
            msg.currentText = msg.fullText.substr(0, msg.charIndex);
            
            HWND hChat = pet->hChatWnd;
            if (hChat) {
                PostMessageW(hChat, WM_USER + 101, 0, 0);
            }
        }
        
        char currentChar = pet->chatHistory[messageIndex].fullText[pet->chatHistory[messageIndex].charIndex - 1];
        if ((currentChar & 0x80) != 0) {
            Sleep(80);
        } else if (currentChar == ' ' || currentChar == '\n' || currentChar == '\r') {
            Sleep(150);
        } else {
            Sleep(50);
        }
    }
}

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
    for (const auto& msg : chatHistory) {
        if (msg.isUser) {
            ss << "You: " << msg.currentText << "\r\n";
        } else {
            ss << "Amiya: " << msg.currentText << (msg.isComplete ? "" : "...") << "\r\n";
        }
        ss << "\r\n";
    }
    
    std::string chatText = ss.str();
    std::wstring wChatText = utf8ToWide(chatText);
    SetWindowTextW(hHistory, wChatText.c_str());
    SendMessageW(hHistory, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(hHistory, EM_SCROLLCARET, 0, 0);
}

void DesktopPet::SendMessageToAI(const std::string& message) {
    // 双重防护：互斥锁 + 时间间隔检查（3秒内不重复触发）
    std::lock_guard<std::mutex> lock(interactMutex);
    uint64_t now = GetCurrentTimeMs();
    if (now - lastTriggerTime < 3000) {
        return;
    }
    lastTriggerTime = now;
    
    isProcessingMessage = true;
    
    std::string cleanedMessage = message;
    size_t pos;
    while ((pos = cleanedMessage.find("\r\n")) != std::string::npos) {
        cleanedMessage.replace(pos, 2, " ");
    }
    while ((pos = cleanedMessage.find("\n")) != std::string::npos) {
        cleanedMessage.replace(pos, 1, " ");
    }
    
    AddChatMessage(cleanedMessage, true);
    std::thread(AIRequestThread, this, cleanedMessage).detach();
    
    // 强制切换到interact并重置动画
    pet.state = interact;
    pet.vx = pet.vy = 0;
    pet.stateTimeLeft = 120;  // 3秒动画时长
    stateFrameIndex[interact] = 0;
    lastState = interact;
    
    // 动画结束后才解锁
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        isProcessingMessage = false;
    }).detach();
}

void DesktopPet::AIRequestThread(DesktopPet* pet, const std::string& message) {
    std::string response = pet->CallOpenAIAPI(message);
    pet->AddChatMessage(response, false);
}

std::string DesktopPet::CallOpenAIAPI(const std::string& prompt) {
    try {
        std::string systemPrompt = "You are Amiya from Arknights. Stay in character and reply as her. Respond naturally to the Doctor's messages. Just output one or two sentences to make your answers brief. Do not start your responses with blank lines or extra whitespace.";
        std::string backgroundPrompt = "Welcome home, Doctor! Hi, Amiya. How are you? I'm doing great, Doctor! My team just recently helped defend Lungmen from a Reunion attack. This fight was one of the biggest we've had yet, but thanks to Rhodes Island we were able to hold the line. Good. I'm glad to hear that. By the way I just bought snacks for the operators. For lunch time. Oh, are those the snacks that Ptilopsis is making now? How do the rest of the operators like them? So true. Luckily I got it before it ran out. You can leave that to Wafarin, Amiya. By the way, did Kal'tsit call me while I was away? Thank you for reminding me to talk to Wafarin about that. As for Kal'tsit, I'm afraid I have bad news. Her calls, and communications with the others, are being blocked or intercepted. As you know, she is very private about her past, but from what her messages said, she was investigating one of her old contacts who may have known something about her past. Since then, it's been radio silence. Same thing, Amiya. Well, I do have an idea of where she might be. There's a large cave just south of Rhodes Island. It's been sealed up for a long while. According to Kal'tsit's information, it was an old Reunion base. She thinks that, maybe, whoever is blocking her communications is located there. If we could locate and eliminate this new threat, Kal'tsit might finally be able to come back! Okay. I'll see him soon with another operator. And I ask that you stay here guarding the base, Amiya. While I go to the location. I feel like I should follow you there, but you're right. The base is still full of Reunion troops. The new threat is still there but unseen. It's possible that they could attack the base during the operation. This base is important, Doctor. It is a central point in our logistics network.";
        
        std::string json = "{\"model\": \"" + MODEL_NAME + "\", \"messages\": [";
        json += "{\"role\": \"system\", \"content\": \"" + escapeJson(systemPrompt) + "\"},";
        json += "{\"role\": \"assistant\", \"content\": \"" + escapeJson(truncateContent(backgroundPrompt)) + "\"},";
        json += "{\"role\": \"user\", \"content\": \"" + escapeJson(prompt) + "\"}";
        json += "]}";
        
        std::string authHeader = "Authorization: Bearer " + API_KEY;
        std::string response = httpPostRequest(API_URL, json, "appl/ation/json", authHeader);
        
        if (response.empty()) return "[Error: No response from AI service]";
        return parseOpenAIResponse(response);
    } catch (...) {
        return "[Error: An error occurred while processing your request]";
    }
}

DesktopPet::DesktopPet(int width, int height) 
    : windowWidth(width), windowHeight(height), isSitting(false), dragging(false),
      showMenu(false), hMouseHook(NULL), isTyping(false),
      sitButtonRel{10, 10, 110, 40}, chatButtonRel{10, 50, 110, 80} {
    instance = this;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    for (int i = 0; i < 4; i++) {
        stateFrameIndex[i] = 0;
    }
    
    pet = {
        (float)(screenW / 2 - windowWidth / 2),
        (float)(screenH / 2 - windowHeight / 2),
        0, 0, 3.0f, 0, 0, 0.3f,
        true, relax, 100
    };
    
    InitializeWindow();
    LoadResources();
    hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(NULL), 0);
    lastPetRect = GetPetWindowRect();
}

DesktopPet::~DesktopPet() {
    chatThreadRunning = false;
    if (hChatWnd) PostMessageW(hChatWnd, WM_QUIT, 0, 0);
    if (chatThread.joinable()) chatThread.join();
    
    if (hMouseHook) UnhookWindowsHookEx(hMouseHook);
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < StateFrameCount[j]; k++)
                delete petImages[i][j][k];
    closegraph();
}

void DesktopPet::InitializeWindow() {
    hwnd = initgraph(windowWidth, windowHeight, EW_DBLCLKS | EW_SHOWCONSOLE);
    LONG l_WinStyle = GetWindowLongW(hwnd, GWL_STYLE);
    SetWindowLongW(hwnd, GWL_STYLE, (l_WinStyle | WS_POPUP) & ~WS_CAPTION & ~WS_THICKFRAME & ~WS_BORDER);
    SetWindowLongW(hwnd, GWL_EXSTYLE, GetWindowLongW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    
    pt_src = {0, 0};
    size_wnd = {windowWidth, windowHeight};
    
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
}

void DesktopPet::LoadResources() {
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
    
    char name[256];
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 4; j++)
        {
            int frameCount = StateFrameCount[j];
            for (int k = 0; k < frameCount; k++)
            {
                sprintf(name, "%s%03d.png", path[i][j], k);
                petImages[i][j][k] = new IMAGE();
                loadimage(petImages[i][j][k], name, windowWidth, windowHeight);
            }
        }
}

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

void DesktopPet::HideChatWindow() {
    HWND hChat = hChatWnd;
    if (hChat) {
        ShowWindow(hChat, SW_HIDE);
        showChatWindow = false;
    }
}

void DesktopPet::ToggleChatWindow() {
    if (showChatWindow) HideChatWindow();
    else CreateChatWindow();
}

void DesktopPet::UpdatePetState() {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    switch (pet.state) {
        case interact:
            // 动画播放期间锁定状态，不允许被其他逻辑打断
            pet.stateTimeLeft--;
            if (pet.stateTimeLeft <= 0) {
                pet.state = (showChatWindow || isSitting) ? sit : relax;
                pet.stateTimeLeft = rand() % 71 + 30;
            }
            break;
        case relax:
            pet.vx *= 0.9f;
            pet.vy *= 0.9f;
            if (fabsf(pet.vx) < 0.1f) pet.vx = 0;
            if (fabsf(pet.vy) < 0.1f) pet.vy = 0;
            
            pet.stateTimeLeft--;
            if (pet.stateTimeLeft <= 0) {
                pet.state = isSitting ? sit : move;
                if (!isSitting) {
                    pet.stateTimeLeft = rand() % 151 + 50;
                    pet.ax = ((float)rand() / RAND_MAX) * 2 * pet.a - pet.a;
                    pet.ay = ((float)rand() / RAND_MAX) * 2 * pet.a - pet.a;
                    pet.facingRight = pet.ax > 0;
                } else {
                    pet.stateTimeLeft = rand() % 71 + 30;
                }
            }
            break;
        case move:
            if (isSitting) {
                pet.state = sit;
                pet.stateTimeLeft = rand() % 71 + 30;
                break;
            }
            
            if (rand() % 30 == 0) {
                pet.ax += ((float)rand() / RAND_MAX) * 0.1f - 0.05f;
                pet.ay += ((float)rand() / RAND_MAX) * 0.1f - 0.05f;
                pet.ax = std::max(-pet.a, std::min(pet.a, pet.ax));
                pet.ay = std::max(-pet.a, std::min(pet.a, pet.ay));
                pet.facingRight = pet.ax > 0;
            }
            
            pet.vx += pet.ax;
            pet.vy += pet.ay;
            pet.vx = std::max(-pet.vmax, std::min(pet.vmax, pet.vx));
            pet.vy = std::max(-pet.vmax, std::min(pet.vmax, pet.vy));
            
            pet.x += pet.vx;
            pet.y += pet.vy;
            
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
            
            pet.stateTimeLeft--;
            if (pet.stateTimeLeft <= 0) {
                pet.state = relax;
                pet.stateTimeLeft = rand() % 71 + 30;
                pet.ax = -pet.vx * 0.1f;
                pet.ay = -pet.vy * 0.1f;
            }
            break;
        case sit:
            pet.vx = 0;
            pet.vy = 0;
            pet.ax = 0;
            pet.ay = 0;
            
            pet.stateTimeLeft--;
            if (pet.stateTimeLeft <= 0) {
                if (isSitting) {
                    pet.stateTimeLeft = rand() % 71 + 30;
                } else {
                    pet.state = relax;
                    pet.stateTimeLeft = rand() % 71 + 30;
                }
            }
            break;
    }
    
    if (showChatWindow && (pet.state == move || dragging)) {
        SendChatWindowPosUpdate();
    }
    
    if (pet.state != lastState) {
        stateFrameIndex[pet.state] = 0;
        if (pet.state == sit) {
            stateFrameIndex[sit] = 0;
        }
        lastState = pet.state;
    }
}

void DesktopPet::HandleInput() {
    ExMessage m;
    while (peekmessage(&m, EM_MOUSE | EM_KEY)) {
        switch (m.message) {
            case WM_LBUTTONUP:
                dragging = false;
                ReleaseCapture();
                break;
            case WM_KEYDOWN:
                if (m.vkcode == VK_ESCAPE) {
                    closegraph();
                    exit(0);
                }
                break;
        }
    }
    
    if (dragging) {
        pet.state = relax;
        POINT curPos;
        GetCursorPos(&curPos);
        int dx = curPos.x - lastMousePos.x;
        int dy = curPos.y - lastMousePos.y;
        
        RECT rect;
        GetWindowRect(hwnd, &rect);
        SetWindowPos(hwnd, 0, rect.left + dx, rect.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        
        lastMousePos = curPos;
        pet.x = (float)(rect.left + dx);
        pet.y = (float)(rect.top + dy);
    }
}

LRESULT CALLBACK DesktopPet::MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (instance) return instance->HandleMouseHookInternal(nCode, wParam, lParam);
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

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

void DesktopPet::Draw() {
    int dir = pet.facingRight ? 0 : 1;
    IMAGE tmp(windowWidth, windowHeight);
    SetWorkingImage(&tmp);
    cleardevice();
    
    putimage(0, 0, petImages[dir][pet.state][stateFrameIndex[pet.state]]);
    
    if (showMenu) {
        setfillcolor(RGB(240, 240, 240));
        solidrectangle(menuRect.left, menuRect.top, menuRect.right, menuRect.bottom);
        setlinecolor(RGB(180, 180, 180));
        rectangle(menuRect.left, menuRect.top, menuRect.right, menuRect.bottom);
        
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
    
    SetWorkingImage(NULL);
    HDC hdcTmp = GetImageHDC(&tmp);
    UpdateLayeredWindow(hwnd, NULL, NULL, &size_wnd, hdcTmp, &pt_src, 0, &blend, ULW_ALPHA);
}

void DesktopPet::Run() {
    while (true) {
        int start = clock();
        HandleInput();
        UpdatePetState();
        SetWindowPos(hwnd, 0, (int)pet.x, (int)pet.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        Draw();
        
        int currentState = pet.state;
        stateFrameIndex[currentState]++;
        if (stateFrameIndex[currentState] >= StateFrameCount[currentState]) {
            stateFrameIndex[currentState] = 0;
        }
        
        int ft = clock() - start;
        if (FrameDelay - ft > 0) Sleep(FrameDelay - ft);
    }
}

extern "C" __declspec(dllimport) BOOL WINAPI SetProcessDPIAware();

int main() {
#ifdef _WIN32
    SetProcessDPIAware();
#endif
    
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, "en_US.UTF-8");
    
    srand((unsigned int)time(NULL));
    DesktopPet pet(300, 300);
    pet.Run();
    return 0;
}
    