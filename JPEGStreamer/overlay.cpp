
#include <opencv2/opencv.hpp>
#include <unordered_map>
#include <string>

using namespace cv;
using namespace std;

// 5x7 font bitmap
static const uint8_t font5x7[][7] = {
    {0x1F,0x05,0x05,0x1F,0x00,0x00,0x00}, // A
    {0x1F,0x15,0x15,0x0A,0x00,0x00,0x00}, // B
    {0x0E,0x11,0x11,0x11,0x00,0x00,0x00}, // C
    {0x1F,0x11,0x11,0x0E,0x00,0x00,0x00}, // D
    {0x1F,0x15,0x15,0x11,0x00,0x00,0x00}, // E
    {0x1F,0x05,0x05,0x01,0x00,0x00,0x00}, // F
    {0x0E,0x11,0x15,0x1D,0x00,0x00,0x00}, // G
    {0x1F,0x04,0x04,0x1F,0x00,0x00,0x00}, // H
    {0x11,0x1F,0x11,0x00,0x00,0x00,0x00}, // I
    {0x08,0x10,0x10,0x0F,0x00,0x00,0x00}, // J
    {0x1F,0x04,0x0A,0x11,0x00,0x00,0x00}, // K
    {0x1F,0x10,0x10,0x10,0x00,0x00,0x00}, // L
    {0x1F,0x02,0x04,0x02,0x1F,0x00,0x00}, // M
    {0x1F,0x02,0x04,0x1F,0x00,0x00,0x00}, // N
    {0x0E,0x11,0x11,0x0E,0x00,0x00,0x00}, // O
    {0x1F,0x05,0x05,0x02,0x00,0x00,0x00}, // P
    {0x0E,0x11,0x19,0x1E,0x00,0x00,0x00}, // Q
    {0x1F,0x05,0x0D,0x12,0x00,0x00,0x00}, // R
    {0x12,0x15,0x15,0x09,0x00,0x00,0x00}, // S
    {0x01,0x1F,0x01,0x00,0x00,0x00,0x00}, // T
    {0x0F,0x10,0x10,0x0F,0x00,0x00,0x00}, // U
    {0x07,0x08,0x10,0x08,0x07,0x00,0x00}, // V
    {0x1F,0x08,0x04,0x08,0x1F,0x00,0x00}, // W
    {0x11,0x0A,0x04,0x0A,0x11,0x00,0x00}, // X
    {0x01,0x02,0x1C,0x02,0x01,0x00,0x00}, // Y
    {0x19,0x15,0x13,0x00,0x00,0x00,0x00}, // Z
    {0x0E,0x11,0x11,0x0E,0x00,0x00,0x00}, // 0
    {0x12,0x1F,0x10,0x00,0x00,0x00,0x00}, // 1
    {0x1A,0x15,0x15,0x12,0x00,0x00,0x00}, // 2
    {0x11,0x15,0x15,0x0A,0x00,0x00,0x00}, // 3
    {0x07,0x04,0x1F,0x04,0x00,0x00,0x00}, // 4
    {0x17,0x15,0x15,0x09,0x00,0x00,0x00}, // 5
    {0x0E,0x15,0x15,0x08,0x00,0x00,0x00}, // 6
    {0x01,0x01,0x1D,0x03,0x00,0x00,0x00}, // 7
    {0x0A,0x15,0x15,0x0A,0x00,0x00,0x00}, // 8
    {0x02,0x15,0x15,0x0E,0x00,0x00,0x00}, // 9
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x00,0x0A,0x00,0x00,0x00,0x00,0x00}, // :
    {0x04,0x04,0x04,0x00,0x00,0x00,0x00}, // -
};

unordered_map<char,int> charMap = {
    {'A',0},{'B',1},{'C',2},{'D',3},{'E',4},{'F',5},{'G',6},{'H',7},{'I',8},{'J',9},
    {'K',10},{'L',11},{'M',12},{'N',13},{'O',14},{'P',15},{'Q',16},{'R',17},{'S',18},{'T',19},
    {'U',20},{'V',21},{'W',22},{'X',23},{'Y',24},{'Z',25},
    {'0',26},{'1',27},{'2',28},{'3',29},{'4',30},{'5',31},{'6',32},{'7',33},{'8',34},{'9',35},
    {' ',36},{':',37},{'-',38}
};

enum OverlayPos {
    TOP_LEFT,    TOP_CENTER,    TOP_RIGHT,
    CENTER_LEFT, CENTER_CENTER, CENTER_RIGHT,
    BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT
};

// Draw a single character
void drawCharOld(Mat &img, char c, int x, int y, Vec3b color, int scale=1) {
    c = toupper(c);
    if(charMap.find(c) == charMap.end()) return;
    int idx = charMap[c];
    for(int row=0; row<7; row++) {
        uint8_t bits = font5x7[idx][row];
        for(int col=0; col<5; col++) {
            if(bits & (1 << (4-col))) {
                for(int dy=0; dy<scale; dy++)
                    for(int dx=0; dx<scale; dx++)
                        if(y+row*scale+dy < img.rows && x+col*scale+dx < img.cols)
                            img.at<Vec3b>(y+row*scale+dy, x+col*scale+dx) = color;
            }
        }
    }
}

void drawChar(Mat &img, char c, int x, int y, Vec3b color, int scale=1) {
    c = toupper(c);
    if(charMap.find(c) == charMap.end()) return;
    int idx = charMap[c];

    for(int col=0; col<5; col++) {               // font columns
        uint8_t bits = font5x7[idx][col];        // get the column bits
        for(int row=0; row<7; row++) {           // font rows
            if(bits & (1 << row)) {              // flip vertically: LSB = top pixel
                for(int dy=0; dy<scale; dy++)
                    for(int dx=0; dx<scale; dx++)
                        if(y+row*scale+dy < img.rows && x+col*scale+dx < img.cols)
                            img.at<Vec3b>(y+row*scale+dy, x+col*scale+dx) = color;
            }
        }
    }
}

// Draw full text string
void drawText(Mat &img, const string &text, int x, int y, Vec3b color, int scale=1, int spacing=1) {
    int startX = x;
    for(char c : text) {
        drawChar(img, c, startX, y, color, scale);
        startX += (5*scale + spacing);
    }
}

// Overlay semi-transparent rectangle + text
void overlayTextSmall(Mat &img, const string &text, int x, int y, Vec3b color, int scale=1) {
    int width = text.length() * (5*scale + 1);
    int height = 7*scale;

    // Clip to image bounds
    width = min(width, img.cols - x);
    height = min(height, img.rows - y);

    // Draw semi-transparent black rectangle
    float alpha = 0.5f;
    for(int row=0; row<height; row++)
        for(int col=0; col<width; col++) {
            Vec3b &px = img.at<Vec3b>(y+row, x+col);
            for(int k=0; k<3; k++)
                px[k] = saturate_cast<uchar>(px[k]*(1-alpha) + 0*alpha);
        }

    // Draw text on top
    drawText(img, text, x, y, color, scale);
}

void overlayTextType2(cv::Mat &img, const std::string &text, int x, int y, cv::Vec3b color, int scale = 1) 
{
    // 1. Convert Vec3b to Scalar (BGR)
    cv::Scalar textColor(color[0], color[1], color[2]);
    
    // 2. Settings for the CCTV look
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = (double)scale * 0.5; // Scaling down to typical HUD size
    int thickness = (scale < 2) ? 1 : 2;
    int baseline = 0;

    // 3. Calculate text size for the background box
    cv::Size textSize = cv::getTextSize(text, fontFace, fontScale, thickness, &baseline);
    
    // Define the rectangle area (padding of 5 pixels)
    cv::Rect bgRect(x - 5, y - textSize.height - 5, textSize.width + 10, textSize.height + baseline + 10);

    // 4. Boundary Check (Crucial for Mat stability)
    bgRect &= cv::Rect(0, 0, img.cols, img.rows);

    // 5. Apply Semi-Transparent Background
    // We create a ROI (Region of Interest) and blend it with black
    if (bgRect.width > 0 && bgRect.height > 0) {
        cv::Mat roi = img(bgRect);
        cv::Mat blackPlate(roi.size(), roi.type(), cv::Scalar(0, 0, 0));
        double alpha = 0.4; // 40% black, 60% original image
        cv::addWeighted(blackPlate, alpha, roi, 1.0 - alpha, 0, roi);
    }

    // 6. Draw the Text (Anti-aliased for a clean look)
    cv::putText(img, text, cv::Point(x, y), fontFace, fontScale, textColor, thickness, cv::LINE_AA);
}

void overlayText(cv::Mat &img, const std::string &text, int x, int y, cv::Vec3b color, int scale = 1) 
{
    // 1. Settings
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = (double)scale * 0.5;
    int thickness = (scale < 2) ? 1 : 2;
    int baseline = 0;

    // 2. Calculate text size
    cv::Size textSize = cv::getTextSize(text, fontFace, fontScale, thickness, &baseline);
    
    // 3. Define Background Box with Padding
    // We use cv::Rect(x, y, w, h) logic
    cv::Rect bgRect(x - 5, y - textSize.height - 5, textSize.width + 10, textSize.height + baseline + 10);

    // 4. Boundary Check (Crucial)
    // This ensures we don't try to access pixels outside the image frame
    bgRect &= cv::Rect(0, 0, img.cols, img.rows);

    // 5. Apply Semi-Transparent Background (Optimized)
    if (bgRect.width > 0 && bgRect.height > 0) {
        cv::Mat roi = img(bgRect);
        // Multiply by 0.6 to darken by 40%. Faster than addWeighted.
        roi *= 0.6; 
    }

    // 6. Draw the Text
    // Now safe to use cv::LINE_AA since headers match 4.8.0!
    cv::putText(img, text, cv::Point(x, y), fontFace, fontScale, 
                cv::Scalar(color[0], color[1], color[2]), thickness, cv::LINE_AA);
}

void overlayTextSmart(cv::Mat &img, const std::string &text, OverlayPos pos= BOTTOM_LEFT, int padding=15, cv::Vec3b color = cv::Vec3b(255,255,255), int scale = 0) 
{
    const int baseThickness = 2;
    const double baseScale = 0.8;

    // 1. Setup Font
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = (double)scale * baseScale;
    int thickness = (scale < 2) ? 1 : 2;
    int baseline = 0;

    //int height = img.rows;
    //int width = img.cols;

    if(img.rows <= 64 || img.cols <= 64)
        return;

    if(scale == 0)
    {
        fontScale = (img.rows / 720.0) * baseScale;
        thickness = std::max(1, (int)((img.rows / 720.0) * baseThickness));
    }

    // 2. Get Text Dimensions
    cv::Size textSize = cv::getTextSize(text, fontFace, fontScale, thickness, &baseline);
    int textW = textSize.width;
    int textH = textSize.height;

    // 3. Calculate X and Y based on Enum
    int x, y;

    // Determine X (Horizontal)
    switch (pos) 
    {
        case TOP_LEFT:    case CENTER_LEFT:   case BOTTOM_LEFT:
            x = padding; 
            break;
        case TOP_CENTER:  case CENTER_CENTER: case BOTTOM_CENTER:
            x = (img.cols - textW) / 2; 
            break;
        case TOP_RIGHT:   case CENTER_RIGHT:  case BOTTOM_RIGHT:
            x = img.cols - textW - padding; 
            break;
        default:
            x = img.cols - textW - padding; 
            break;

    }

    // Determine Y (Vertical) - Note: putText Y is the baseline (bottom)
    switch (pos) {
        case TOP_LEFT:    case TOP_CENTER:    case TOP_RIGHT:
            y = padding + textH; 
            break;
        case CENTER_LEFT: case CENTER_CENTER: case CENTER_RIGHT:
            y = (img.rows + textH) / 2; 
            break;
        case BOTTOM_LEFT: case BOTTOM_CENTER: case BOTTOM_RIGHT:
            y = img.rows - padding - baseline; 
            break;
        default:
            y = img.rows - padding - baseline; 
            break;
    }

    // 4. Draw Background Box (Using the logic from before)
    cv::Rect bgRect(x - 5, y - textH - 5, textW + 10, textH + baseline + 10);
    bgRect &= cv::Rect(0, 0, img.cols, img.rows); // Safety check

    if (bgRect.width > 0 && bgRect.height > 0) 
    {
        img(bgRect) *= 0.6; // Darken background
    }

    // 5. Render Text
    cv::putText(img, text, cv::Point(x, y), fontFace, fontScale, 
                cv::Scalar(color[0], color[1], color[2]), thickness, cv::LINE_AA);

    return;
}

void overlayTextPro(cv::Mat &img, const std::string &text, OverlayPos pos = BOTTOM_LEFT, int padding = 15, cv::Vec3b color=cv::Vec3b(255,255,255), int scale = 1, bool outline = true) 
{
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = (double)scale * 0.8;
    int thickness = (scale < 2) ? 1 : 2;
    int baseline = 0;

    cv::Size textSize = cv::getTextSize(text, fontFace, fontScale, thickness, &baseline);
    
    // Calculate X and Y (Reuse the switch-case logic from before)
    int x, y;
    // ... [Horizontal Logic] ...
    if (pos == TOP_LEFT || pos == CENTER_LEFT || pos == BOTTOM_LEFT) x = padding;
    else if (pos == TOP_CENTER || pos == CENTER_CENTER || pos == BOTTOM_CENTER) x = (img.cols - textSize.width) / 2;
    else x = img.cols - textSize.width - padding;

    // ... [Vertical Logic] ...
    if (pos == TOP_LEFT || pos == TOP_CENTER || pos == TOP_RIGHT) y = padding + textSize.height;
    else if (pos == CENTER_LEFT || pos == CENTER_CENTER || pos == CENTER_RIGHT) y = (img.rows + textSize.height) / 2;
    else y = img.rows - padding - baseline;

    // 1. Draw Background Box (Optional, but still good for contrast)
    cv::Rect bgRect(x - 5, y - textSize.height - 5, textSize.width + 10, textSize.height + baseline + 10);
    bgRect &= cv::Rect(0, 0, img.cols, img.rows);
    if (bgRect.width > 0 && bgRect.height > 0) {
        img(bgRect) *= 0.7; // Subtle darken
    }

    // 2. Draw OUTLINE (The "Halo" Effect)
    if (outline) {
        // Draw black text with double thickness to create the border
        cv::putText(img, text, cv::Point(x, y), fontFace, fontScale, 
                    cv::Scalar(0, 0, 0), thickness + 2, cv::LINE_AA);
    }

    // 3. Draw Main Colored Text
    cv::putText(img, text, cv::Point(x, y), fontFace, fontScale, 
                cv::Scalar(color[0], color[1], color[2]), thickness, cv::LINE_AA);
}

int testRectangle()
{
    cv::Mat test(500, 500, CV_8UC3, cv::Scalar(0,0,0));

    // Test A: Basic OpenCV Rectangle
    cv::rectangle(test, cv::Rect(100, 100, 300, 300), cv::Scalar(255, 255, 255), -1);

    // Test B: Manual Pixel Access (The 'Nuclear' Option)
    test.at<cv::Vec3b>(250, 250) = cv::Vec3b(255, 255, 255);

    int nz = cv::countNonZero(test.reshape(1));
    std::cout << "Non-zero count: " << nz << "\n";

    return 1;
}

void displayOpenCVDetails()
{
    std::cout << "OpenCV Header version: " << CV_VERSION << "\n";
    std::cout << "OpenCV Library version: " << cv::getVersionString() << "\n";
}

void mainOpenCVDiagnostics() 
{
    // 1. Create the Mat
    cv::Mat test(500, 500, CV_8UC3, cv::Scalar(0,0,0));

    // 2. CHECK: Is the data pointer actually valid?
    if (test.empty() || !test.data) 
    {
        std::cout << "CRITICAL: Mat failed to allocate memory!" << "\n";
        return;
    }

    std::cout << "Size: " << test.cols << "x" << test.rows << "\n";
    std::cout << "Channels: " << test.channels() << "\n";
    std::cout << "Step: " << test.step << " bytes per row" << "\n";

    // 3. Test B: Use a raw pointer instead of .at (Manual memory write)
    try 
    {
        uchar* pixelPtr = test.ptr<uchar>(250); // Get pointer to row 250
        pixelPtr[250 * 3 + 0] = 255; // Set Blue channel to 255
        std::cout << "Manual memory write successful." << "\n";
    } 
    catch (const std::exception& e) 
    {
        std::cout << "Crash during manual write: " << e.what() << "\n";
    }

    // 4. Test A: OpenCV internal function
    cv::rectangle(test, cv::Rect(0, 0, 10, 10), cv::Scalar(255, 255, 255), -1);
    std::cout << "OpenCV Rectangle successful." << "\n";

    return;
}

int textOverlayDiagnostics(const char *image_name)
{
    // Create a 500x500 black image
    cv::Mat test = cv::Mat::zeros(500, 500, CV_8UC3);

    // Draw a big white circle first (to test if drawing works at all)
    cv::circle(test, cv::Point(250, 250), 100, cv::Scalar(255, 255, 255), -1);

    // Draw the text
    cv::putText(test, "TEST", cv::Point(150, 250), cv::FONT_HERSHEY_SIMPLEX, 2.0, cv::Scalar(0, 0, 255), 3);

    // Check memory: count non-zero pixels
    int nonZero = cv::countNonZero(test.reshape(1)); 
    std::cout << "Non-zero pixels: " << nonZero << "\n";

    int ret = -1;
    if (nonZero == 0) 
    {
        std::cerr << "CRITICAL: The Mat is still empty. Drawing functions are not working." << "\n";
        ret = -1;
    } 
    else 
    {
        bool success = cv::imwrite(image_name, test);
        if (success) 
        {
            ret = 0;
            std::cout << "Success! Check :" << image_name << "\n";
        }    
        else 
        {
            ret = -2;
            std::cerr << "Failed to save file: " << image_name << "Check folder permissions." << "\n";
        }            
    }

    return ret;
}

void testClassicalBlockOverlay(const char *image_name)
{
    Mat frame(720, 1280, CV_8UC3, Scalar(50,50,50));
    string ts = "OCT 7 2025 12:34:56";
    overlayTextSmall(frame, ts, 20, 680, Vec3b(255,255,255), 3);
    bool success = cv::imwrite(image_name, frame);

    if(success) 
    {
        std::cout << "File saved successfully in: " << image_name << "\n";
    } 
    else 
    {
        std::cerr << "Error: Could not save: " << image_name << " Check folder permissions." << "\n";
    }

    return;
}

void createCCTVOverlays(const char *image_name)
{   
    // 1. Create a 720p frame (Height, Width)
    //cv::Mat frame = cv::Mat::zeros(720, 1280, CV_8UC3);
    Mat frame(720, 1280, CV_8UC3, Scalar(0,0,0));

    // 2. Draw a simple pattern so we can test transparency
    for(int i = 0; i < frame.cols; i += 40)
        cv::line(frame, cv::Point(i, 0), cv::Point(i, frame.rows), cv::Scalar(50, 50, 50), 1);

    // 3. Define our test parameters
    cv::Vec3b cctvYellow(0, 255, 255); // BGR
    std::string timestamp = "CAM-04  2026-02-28 23:51:04  [REC]";

    // 4. Call your function
    // Testing at the top-left (typical CCTV spot)
    overlayText(frame, timestamp, 50, 60, cctvYellow, 1);

    // Testing a larger scale in the center
    overlayText(frame, "MOTION DETECTED - ZONE 1", 400, 360, cv::Vec3b(0, 0, 255), 2);

    bool success = cv::imwrite(image_name, frame);
    
    if(success) 
    {
        std::cout << "File saved successfully in: " << image_name << "\n";
    } 
    else 
    {
        std::cerr << "Error: Could not save the image: << " << image_name << ". Check folder permissions." << "\n";
    }

    return;
}

void testMtxCamTimestampOverlay(const char *image_name)
{
    Mat frame(720, 1280, CV_8UC3, Scalar(50,50,50));
    string ts = "2026-02-28 23:51:04";

    overlayText(frame, ts, 20, 680, Vec3b(255,255,255), 1);
    bool success = cv::imwrite(image_name, frame);

    if(success) 
    {
        std::cout << "File saved successfully in: " << image_name << "\n";
    } 
    else 
    {
        std::cerr << "Error: Could not save: " << image_name << ". Check folder permissions." << "\n";
    }

    return;
}
void testMtxCamTimestampPosOverlay(const char *image_name)
{
    Mat frame(720, 1280, CV_8UC3, Scalar(128,128,128));
    string tL = "TL: CamName";
    string ts = "TS: 2026-02-28 23:51:04";

    //overlayTextPro(cv::Mat &img, const std::string &text, OverlayPos pos, int padding, cv::Vec3b color, int scale = 1, bool outline = true) 
    overlayTextPro(frame, ts, OverlayPos::BOTTOM_LEFT);
    overlayTextPro(frame, tL, OverlayPos::TOP_LEFT, 20, Vec3b(255,255,255), 2);
    overlayTextSmart(frame, tL, OverlayPos::BOTTOM_RIGHT, 20, Vec3b(255,255,255), 1.5);
    bool success = cv::imwrite(image_name, frame);

    if(success) 
    {
        std::cout << "File saved successfully in: " << image_name << "\n";
    } 
    else 
    {
        std::cerr << "Error: Could not save: " << image_name << ". Check folder permissions." << "\n";
    }

    return;
}

int testOpenCVFunctions()
{
    displayOpenCVDetails();

    testClassicalBlockOverlay("IMG-overlayTextSmall.jpg");

    mainOpenCVDiagnostics();

    textOverlayDiagnostics("IMG-font_test.jpg");

    testMtxCamTimestampOverlay("IMG-timestamp_overlay.jpg");

    createCCTVOverlays("IMG-cctv_test_overlays.jpg");

    testMtxCamTimestampPosOverlay("IMG-testMtxCamTimestampPosOverlay.jpg");

    return 0;
}

int displayTextOnImage(cv::Mat &img, const std::string &text)
{
    //overlayTextPro(img, text);
    overlayTextSmart(img, text);

    return 0;
}

/*int main() {
    Mat frame(720, 1280, CV_8UC3, Scalar(50,50,50));
    string ts = "OCT 7 2025 12:34:56";
    overlayText(frame, ts, 20, 680, Vec3b(255,255,255), 3);

    imwrite("timestamp_overlay.jpg", frame);
    return 0;
}*/
