#include <GL/glut.h>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm> // For std::clamp, std::min, std::max
#include <iostream>  // For std::cout, std::cerr
#ifdef _WIN32
  #include <windows.h> // For Windows-specific API calls
#endif
using namespace std;

// Simple system monitor dashboard using GLUT.

// ----- Configurable Constants -----
const int INIT_W = 1200, INIT_H = 800;
const int HEADER_H = 70, FOOTER_H = 40, ALERT_H = 50;
const int MARGIN = 25;
const int MAX_HISTORY = 60; // How many data points for history graphs
const float METRIC_BAR_WIDTH_RATIO = 0.7f;

// ----- Color Definitions -----
const float COL_BG[4]          = {0.09f, 0.09f, 0.12f, 1.0f};
const float COL_HEADER[4]      = {0.15f, 0.15f, 0.20f, 1.0f};
const float COL_FOOTER[4]      = {0.12f, 0.12f, 0.16f, 1.0f};
const float COL_PANEL[4]       = {0.13f, 0.13f, 0.17f, 1.0f};
const float COL_ALERT[4]       = {0.85f, 0.20f, 0.20f, 0.85f};
const float COL_TEXT_MAIN[3]   = {0.95f, 0.95f, 1.00f};
const float COL_TEXT_SECOND[3] = {0.70f, 0.70f, 0.80f};
const float COL_BORDER[3]      = {0.35f, 0.35f, 0.45f};
const float COL_GRID[4]        = {0.25f, 0.25f, 0.30f, 0.3f};
const float COL_TEXT_HIGHLIGHT[3] = {1.0f, 1.0f, 0.0f}; // Yellow

// ----- Metric Structure -----
struct Metric {
    string name;
    string unit;
    float value;
    float threshold;
    float color[3];
    float darkColor[3]; // For gradient effect
    vector<float> history;
    bool isAlerting = false;
};

// ----- Globals -----
int winW = INIT_W, winH = INIT_H;
bool showHistory = true;
vector<Metric> metrics = {
    {"CPU",     "%", 0, 90, {0.95f, 0.30f, 0.30f}, {0.65f, 0.15f, 0.15f}},
    {"Memory",  "%", 0, 90, {0.30f, 0.60f, 0.95f}, {0.15f, 0.30f, 0.65f}},
    {"Network", "%", 0, 80, {0.30f, 0.95f, 0.40f}, {0.15f, 0.65f, 0.20f}}
};
vector<string> alerts;

// For temporary threshold display
int activeThresholdChangeMetricIndex = -1;
int thresholdDisplayTimer = 0;
const int THRESHOLD_DISPLAY_DURATION_SECONDS = 2;

// ----- Platform Metric Readers -----
#ifdef _WIN32
// Windows CPU reading: compare system times over an interval.
static ULARGE_INTEGER lastCPUIdle, lastCPUKernel, lastCPUUser;// 64 bit unsigned integer
static bool firstCPURead = true;

float readCPU() {
    FILETIME idleTimeFT, kernelTimeFT, userTimeFT;
    if (!GetSystemTimes(&idleTimeFT, &kernelTimeFT, &userTimeFT)) return 0.0f;//Error handling ka kam kar raha

    ULARGE_INTEGER currentIdle = {idleTimeFT.dwLowDateTime, idleTimeFT.dwHighDateTime};
    ULARGE_INTEGER currentKernel = {kernelTimeFT.dwLowDateTime, kernelTimeFT.dwHighDateTime};
    ULARGE_INTEGER currentUser = {userTimeFT.dwLowDateTime, userTimeFT.dwHighDateTime};

    if (firstCPURead) { // Prime the readings
        lastCPUIdle = currentIdle; lastCPUKernel = currentKernel; lastCPUUser = currentUser;
        firstCPURead = false;
        Sleep(100); // Brief pause to get a delta on next call. Only happens once.
        return readCPU();
    }

    ULONGLONG idleDiff = currentIdle.QuadPart - lastCPUIdle.QuadPart;
    ULONGLONG kernelDiff = currentKernel.QuadPart - lastCPUKernel.QuadPart;
    ULONGLONG userDiff = currentUser.QuadPart - lastCPUUser.QuadPart;
    ULONGLONG totalSystem = kernelDiff + userDiff;      // Busy time
    ULONGLONG totalTimeElapsed = totalSystem + idleDiff; // Total time slice

    lastCPUIdle = currentIdle; lastCPUKernel = currentKernel; lastCPUUser = currentUser;
    return (totalTimeElapsed > 0) ? (float)((totalSystem * 100.0) / totalTimeElapsed) : 0.0f;
}
float readMemory() {
    MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms);
    return GlobalMemoryStatusEx(&ms) ?
        (float)((ms.ullTotalPhys - ms.ullAvailPhys) * 100.0 / ms.ullTotalPhys) : 0.0f;
}
#else // Fallback for non-Windows
float readCPU()    { return rand() % 101; }
float readMemory() { return rand() % 101; }
#endif
float readNetwork() { return rand() % 101; } // Random for all platforms for simplicity

// ----- Drawing Utilities -----
void drawText(float x, float y, const string &s, const float col[3], void *font=GLUT_BITMAP_HELVETICA_18) {
    glColor3fv(col); glRasterPos2f(x, y);
    for (char c : s) glutBitmapCharacter(font, c);
}
void drawQuad(float x, float y, float w, float h, const float col[4]) {
    glColor4fv(col); glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x+w, y); glVertex2f(x+w, y+h); glVertex2f(x, y+h);
    glEnd();
}
void drawBorder(float x, float y, float w, float h, const float col[3], float lineW=1.5f) {
    glLineWidth(lineW); glColor3fv(col);
    glBegin(GL_LINE_LOOP); // Closed loop
    glVertex2f(x, y); glVertex2f(x+w, y); glVertex2f(x+w, y+h); glVertex2f(x, y+h);
    glEnd(); glLineWidth(1.0f);
}
void drawGradientBar(float x, float y, float w, float h, float percent, const float topCol[3], const float bottomCol[3]) {
    float fillH = h * percent; // Actual fill height
    glBegin(GL_QUADS);
      glColor3fv(bottomCol); glVertex2f(x, y); glVertex2f(x+w, y);
      glColor3fv(topCol);    glVertex2f(x+w, y+fillH); glVertex2f(x, y+fillH);
    glEnd();
}

// ----- Metrics Update -----
void updateMetrics() {
    alerts.clear();
    metrics[0].value = clamp(readCPU(), 0.0f, 100.0f);
    metrics[1].value = clamp(readMemory(), 0.0f, 100.0f);
    metrics[2].value = clamp(readNetwork(), 0.0f, 100.0f);

    for (auto &m : metrics) {
        m.history.push_back(m.value);
        if (m.history.size() > MAX_HISTORY) m.history.erase(m.history.begin());
        
        m.isAlerting = (m.value > m.threshold && m.threshold > 0); // Alert if over meaningful threshold
        if (m.isAlerting) {
            alerts.push_back("⚠ " + m.name + " alert: " + to_string((int)m.value) +
                           m.unit + " (threshold: " + to_string((int)m.threshold) + m.unit + ")");
        }
    }
}

// ----- Main Display -----
void display() {
    glClearColor(COL_BG[0], COL_BG[1], COL_BG[2], COL_BG[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    drawQuad(0, winH-HEADER_H, winW, HEADER_H, COL_HEADER);
    drawText(MARGIN, winH-HEADER_H/2+6, "SYSTEM MONITOR DASHBOARD", COL_TEXT_MAIN, GLUT_BITMAP_HELVETICA_18);

    float alertSpace = 0;
    if (!alerts.empty()) {
        alertSpace = ALERT_H + 5;//5 representing padding
        float alertY = winH - HEADER_H - ALERT_H;
        drawQuad(MARGIN, alertY, winW-2*MARGIN, ALERT_H, COL_ALERT);
        drawBorder(MARGIN, alertY, winW-2*MARGIN, ALERT_H, COL_BORDER, 2.0f);
        for (size_t i = 0; i < alerts.size(); ++i) {
            drawText(MARGIN+15, alertY+ALERT_H-18-i*18, alerts[i], COL_TEXT_MAIN, GLUT_BITMAP_9_BY_15);
        }
    }

    float contentY = FOOTER_H;
    float contentH = winH - HEADER_H - alertSpace - FOOTER_H - MARGIN;
    float graphH = showHistory ? contentH*0.4f : 0;
    float metricsH = contentH - graphH - (showHistory ? MARGIN : 0);

    if (metricsH > 50) { // Draw metric bars if space permits
        float metricsY = contentY + graphH + (showHistory ? MARGIN : 0);
        float barAreaW = winW - 2*MARGIN;
        float totalBarWidth = barAreaW * METRIC_BAR_WIDTH_RATIO;
        float totalSpacing = barAreaW - totalBarWidth;
        float barSpacing = totalSpacing / (metrics.size() + 1);
        float barW = totalBarWidth / metrics.size();

        for (size_t i = 0; i < metrics.size(); ++i) {
            float barX = MARGIN + barSpacing*(i+1) + barW*i;
            drawQuad(barX, metricsY, barW, metricsH, COL_PANEL);
            drawBorder(barX, metricsY, barW, metricsH, COL_BORDER);
            drawGradientBar(barX, metricsY, barW, metricsH, metrics[i].value/100.0f,
                           metrics[i].color, metrics[i].darkColor);

            bool isTemporarilyDisplayingThreshold = (static_cast<int>(i) == activeThresholdChangeMetricIndex && thresholdDisplayTimer > 0);

            // Draw threshold line: highlighted if actively changing, or normal if set and not alerting.
            if (isTemporarilyDisplayingThreshold || (!metrics[i].isAlerting && metrics[i].threshold > 0 && metrics[i].threshold < 100)) {
                float markerY = metricsY + metricsH * (metrics[i].threshold/100.0f);
                markerY = clamp(markerY, metricsY +1.0f, metricsY + metricsH -1.0f); // Keep inside bar

                float lineWidth = isTemporarilyDisplayingThreshold ? 3.0f : 2.0f;
                const float* lineColor = isTemporarilyDisplayingThreshold ? COL_TEXT_HIGHLIGHT : COL_TEXT_MAIN;
                
                glLineWidth(lineWidth); glColor3fv(lineColor);
                glBegin(GL_LINES); glVertex2f(barX, markerY); glVertex2f(barX+barW, markerY); glEnd();
                glLineWidth(1.0f);

                if (isTemporarilyDisplayingThreshold) { // Show threshold value text
                    string thrStr = to_string((int)metrics[i].threshold) + metrics[i].unit;
                    float textX = barX + barW + 5; // Right of bar
                    float textY = markerY - 7;     // Centered on marker line
                    textY = max(textY, metricsY + 2.0f); textY = min(textY, metricsY + metricsH - 17.0f);
                    drawText(textX, textY, thrStr, COL_TEXT_HIGHLIGHT, GLUT_BITMAP_9_BY_15);
                }
            }

            string valueStr = to_string((int)metrics[i].value) + metrics[i].unit;
            float valueTextW = valueStr.length() * 9;
            drawText(barX + (barW - valueTextW)/2, metricsY + metricsH + 10, valueStr, metrics[i].color);
            float nameTextW = metrics[i].name.length() * 9;
            drawText(barX + (barW - nameTextW)/2, metricsY - 20, metrics[i].name, COL_TEXT_SECOND);
        }
    }

    if (showHistory && graphH > 20) { // Draw history graph if enabled and space
        float graphX = MARGIN; float graphY = contentY; float graphW = winW - 2*MARGIN;
        drawQuad(graphX, graphY, graphW, graphH, COL_PANEL);
        drawBorder(graphX, graphY, graphW, graphH, COL_BORDER);
        
        glColor4fv(COL_GRID); glBegin(GL_LINES); // Horizontal grid lines
        for (int i = 1; i < 5; ++i) { float y = graphY + graphH*i/5.0f; glVertex2f(graphX+1,y); glVertex2f(graphX+graphW-1,y); }
        glEnd();

        glLineWidth(2.0f); // Plot metric history lines
        for (const auto &m : metrics) {
            if (m.history.size() < 2) continue;
            glColor3fv(m.color); glBegin(GL_LINE_STRIP);
            for (size_t i = 0; i < m.history.size(); ++i) {
                float x = graphX + graphW * (float)i/(MAX_HISTORY-1); // Scale X to graph width
                float y_val = graphY + graphH * (m.history[i]/100.0f);   // Scale Y to graph height
                glVertex2f(x, clamp(y_val, graphY, graphY + graphH));
            }
            glEnd();
        }
        glLineWidth(1.0f);

        float legendX = graphX + 10, legendY = graphY + 10; // Graph legend
        for (size_t i=0; i<metrics.size(); ++i) {
            drawText(legendX, legendY, "■", metrics[i].color);
            drawText(legendX + 15, legendY, metrics[i].name, COL_TEXT_SECOND, GLUT_BITMAP_9_BY_15);
            legendX += metrics[i].name.length()*7 + 40;
        }
    }

    drawQuad(0, 0, winW, FOOTER_H, COL_FOOTER);
    drawText(MARGIN, FOOTER_H/2 - 5,
        "H: History | C: Clear | 1/2: CPU Thr | 3/4: Mem Thr | 5/6: Net Thr | E: Export | ESC: Exit",
        COL_TEXT_SECOND, GLUT_BITMAP_9_BY_15);

    glutSwapBuffers();
}

// ----- Interaction -----
void timer(int) {
    updateMetrics();
    if (thresholdDisplayTimer > 0) {
        thresholdDisplayTimer--; // Countdown for temp threshold display
        if (thresholdDisplayTimer == 0) activeThresholdChangeMetricIndex = -1;
    }
    glutPostRedisplay();
    glutTimerFunc(1000, timer, 0); // ~1 FPS
}

void reshape(int w, int h) {
    winW = w; winH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluOrtho2D(0, w, 0, h); // Match pixel coords
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();// translation rotation and scaling
}

void keyboard(unsigned char k, int, int) {
    int metric_idx_to_update = -1; // Helper for threshold keys

    switch(tolower(k)) {
        case 'h': showHistory = !showHistory; break;
        case 'c':
            for (auto &m : metrics) { m.history.clear(); m.isAlerting = false; }
            alerts.clear(); activeThresholdChangeMetricIndex = -1; thresholdDisplayTimer = 0;
            break;
        case 'e': { // Export to CSV
            ofstream f("monitor_log.csv");
            if (!f) { cerr << "Error: Could not open monitor_log.csv" << endl; break; }
            f << "Time(sec)"; for(const auto& m : metrics) f << "," << m.name << "(" << m.unit << ")"; f << "\n";
            if (!metrics.empty() && !metrics[0].history.empty()) {
                for (size_t i = 0; i < metrics[0].history.size(); ++i) {
                    f << i; for(const auto& m : metrics) f << "," << (i < m.history.size() ? m.history[i] : 0.0f); f << "\n";
                }
            }
            cout << "Data exported to monitor_log.csv" << endl; break;
        }
        // Threshold adjustments
        case '1': if (metrics.size()>0) { metrics[0].threshold = min(100.0f, metrics[0].threshold+1); metric_idx_to_update = 0; } break;
        case '2': if (metrics.size()>0) { metrics[0].threshold = max(  0.0f, metrics[0].threshold-1); metric_idx_to_update = 0; } break;
        case '3': if (metrics.size()>1) { metrics[1].threshold = min(100.0f, metrics[1].threshold+1); metric_idx_to_update = 1; } break;
        case '4': if (metrics.size()>1) { metrics[1].threshold = max(  0.0f, metrics[1].threshold-1); metric_idx_to_update = 1; } break;
        case '5': if (metrics.size()>2) { metrics[2].threshold = min(100.0f, metrics[2].threshold+1); metric_idx_to_update = 2; } break;
        case '6': if (metrics.size()>2) { metrics[2].threshold = max(  0.0f, metrics[2].threshold-1); metric_idx_to_update = 2; } break;
        case 27: exit(0); // ESC
    }

    if (metric_idx_to_update != -1) { // If a threshold was changed
        activeThresholdChangeMetricIndex = metric_idx_to_update;
        thresholdDisplayTimer = THRESHOLD_DISPLAY_DURATION_SECONDS;
    }
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    srand(time(0)); // For fallback random data
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_ALPHA);
    glutInitWindowSize(INIT_W, INIT_H);
    glutCreateWindow("Advanced System Monitor v2.1 (Temp Threshold Display)");
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    #ifdef _WIN32
        firstCPURead = true; readCPU(); // Prime Windows CPU reader
    #endif
    glutTimerFunc(100, timer, 0); // Initial delay, then 1s updates
    glutMainLoop();
    return 0;
}