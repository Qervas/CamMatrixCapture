#include "gui/gui_manager.hpp"
#include "gui/main_window.hpp"
#include <SapClassBasic.h>

// Add Windows-specific headers for message filtering
#ifdef _WIN32
#include <windows.h>
#include <winuser.h>
#endif

#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <iostream>
#include <stdexcept>
#include <QTimer>
#include <type_traits>
#include <memory>
#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <QDir>
#include <QTextStream>

// Global file for logging
QFile* g_logFile = nullptr;

// Custom message handler to redirect qDebug to file
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // Create timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    
    // Format message based on type
    QString logMessage;
    switch (type) {
    case QtDebugMsg:
        logMessage = QString("%1 [Debug]: %2").arg(timestamp, msg);
        break;
    case QtInfoMsg:
        logMessage = QString("%1 [Info]: %2").arg(timestamp, msg);
        break;
    case QtWarningMsg:
        logMessage = QString("%1 [Warning]: %2").arg(timestamp, msg);
        break;
    case QtCriticalMsg:
        logMessage = QString("%1 [Critical]: %2").arg(timestamp, msg);
        break;
    case QtFatalMsg:
        logMessage = QString("%1 [Fatal]: %2").arg(timestamp, msg);
        break;
    }
    
    // Output to console
    fprintf(stderr, "%s\n", logMessage.toLocal8Bit().constData());
    
    // Output to log file if available
    if (g_logFile && g_logFile->isOpen()) {
        QTextStream stream(g_logFile);
        stream << logMessage << "\n";
        stream.flush();
    }
}

// SFINAE template techniques for safer Sapera operations
namespace sapera_utils {
    // Check if a type has a specific method (using SFINAE)
    template<typename T>
    class has_create_method {
        typedef char yes[1];
        typedef char no[2];
        
        template<typename C> static yes& test(decltype(&C::Create));
        template<typename C> static no& test(...);
        
    public:
        static constexpr bool value = sizeof(test<T>(0)) == sizeof(yes);
    };
    
    // Safe wrapper for Sapera objects that need creation
    template<typename T>
    typename std::enable_if<has_create_method<T>::value, bool>::type
    safe_create(T* obj) {
        if (!obj) return false;
        try {
            return obj->Create();
        } catch (...) {
            std::cerr << "Exception during Sapera object creation" << std::endl;
            return false;
        }
    }
    
    // Fallback for types without Create method
    template<typename T>
    typename std::enable_if<!has_create_method<T>::value, bool>::type
    safe_create(T*) {
        return true; // No creation needed
    }
}

// Function to close error dialogs by window name - much simpler approach
#ifdef _WIN32
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    char className[256] = {0};
    char windowText[256] = {0};
    
    // Get class name and window text
    GetClassName(hwnd, className, sizeof(className));
    GetWindowText(hwnd, windowText, sizeof(windowText));
    
    // Check for Sapera error dialogs
    if (strstr(className, "#32770") && 
        (strstr(windowText, "Error in") || 
         strstr(windowText, "Sapera LT") || 
         strstr(windowText, "CorAcq"))) {
        
        // Found a dialog that appears to be a Sapera error
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        
        // Make sure it's part of our process
        if (pid == GetCurrentProcessId()) {
            // Click the 'No' button automatically to dismiss the dialog
            HWND noButton = FindWindowEx(hwnd, NULL, "Button", "No");
            if (noButton) {
                PostMessage(noButton, WM_LBUTTONDOWN, 0, 0);
                PostMessage(noButton, WM_LBUTTONUP, 0, 0);
            } else {
                // If can't find No button, just close the dialog
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
        }
    }
    return TRUE; // Continue enumeration
}

// Function to check for and close any Sapera dialogs
void closeSaperaDialogs() {
    EnumWindows(EnumWindowsProc, 0);
}
#endif

int main(int argc, char *argv[]) {
    try {
        // Set up logging to file
        QString logDir = "C:/Users/Radiance/Desktop/CamCaptures";
        QDir dir(logDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        
        // Create log file with timestamp
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        QString logPath = logDir + "/camera_log_" + timestamp + ".txt";
        g_logFile = new QFile(logPath);
        
        if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
            // Install the custom message handler
            qInstallMessageHandler(messageHandler);
            qInfo() << "Log file initialized at:" << logPath;
        } else {
            delete g_logFile;
            g_logFile = nullptr;
            std::cerr << "Could not open log file at: " << logPath.toStdString() << std::endl;
        }
        
        // Early initialization to disable Sapera dialogs
        SapManager::SetDisplayStatusMode(FALSE);
        
        QApplication app(argc, argv);
        
        // Set up a periodic timer to check for and close Sapera error dialogs
#ifdef _WIN32
        QTimer* dialogCheckerTimer = new QTimer(&app);
        QObject::connect(dialogCheckerTimer, &QTimer::timeout, []() {
            // Check for and close any Sapera error dialogs every 200ms
            closeSaperaDialogs();
        });
        dialogCheckerTimer->start(200); // Check every 200 milliseconds
#endif
        
        // Double check that display mode is disabled globally
        SapManager::SetDisplayStatusMode(FALSE);

        // Set application info
        QApplication::setOrganizationName("YourCompany");
        QApplication::setApplicationName("Camera Matrix Capture");
        
        // Apply stylesheet for better appearance
        app.setStyle("Fusion");

        // Create GUI manager
        cam_matrix::GuiManager guiManager;

        // Initialize GUI
        if (!guiManager.initialize()) {
            throw std::runtime_error("Failed to initialize GUI");
        }

        // Show the main window
        auto mainWindow = guiManager.getMainWindow();
        if (mainWindow) {
            mainWindow->show();
            
            // Add a short delay before scanning for cameras to ensure UI is fully initialized
            QTimer::singleShot(500, [mainWindow]() {
                // Trigger camera refresh after UI is shown
                QMetaObject::invokeMethod(mainWindow, "refreshCameras", Qt::QueuedConnection);
            });
        } else {
            throw std::runtime_error("Failed to create main window");
        }

        // Run the application
        return app.exec();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
