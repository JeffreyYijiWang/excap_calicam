
// This is No Warranty No Copyright Software.
// astar.ai
// Nov 16, 2018

#include <opencv2/opencv.hpp>

#include <array>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

////////////////////////////////////////////////////////////////////////////////

bool live = true;
// To run live mode, you need a CaliCam from www.astar.ai

int vfov_bar = 0, width_bar = 0, height_bar = 0;
int vfov_max = 60, width_max = 480, height_max = 360;
int vfov_now = 60, width_now = 480, height_now = 360;

int ndisp_bar = 1, wsize_bar = 2;
int ndisp_max = 2, wsize_max = 4;
int ndisp_now = 32, wsize_now = 7;

int cap_cols, cap_rows, img_width;
bool changed = false;
bool is_sgbm = true;
cv::Mat Translation, Kl, Kr, Dl, Dr, xil, xir, Rl, Rr, smap[2][2], Knew;

std::string cam_model;

enum class DisplayMode
{
  Left = 0,
  Right,
  SideBySide,
  Blend,
  Anaglyph
};

DisplayMode display_mode = DisplayMode::SideBySide;
cv::Scalar left_anaglyph_color(0, 0, 255);  // BGR: red
cv::Scalar right_anaglyph_color(255, 0, 0); // BGR: blue

const std::string controls_window = "CaliCam Controls";
const std::string viewer_window = "Fisheye Viewer";
const std::string anaglyph_window = "3D Anaglyph";

const cv::Rect left_color_button(20, 55, 220, 55);
const cv::Rect right_color_button(260, 55, 220, 55);
const std::array<cv::Rect, 5> mode_buttons = {
    cv::Rect(20, 150, 120, 45),
    cv::Rect(150, 150, 120, 45),
    cv::Rect(280, 150, 120, 45),
    cv::Rect(410, 150, 120, 45),
    cv::Rect(540, 150, 140, 45)};

bool controls_dirty = true;

const char *DisplayModeName(DisplayMode mode)
{
  switch (mode)
  {
  case DisplayMode::Left:
    return "Left";
  case DisplayMode::Right:
    return "Right";
  case DisplayMode::SideBySide:
    return "Side by Side";
  case DisplayMode::Blend:
    return "Blend";
  case DisplayMode::Anaglyph:
    return "Anaglyph";
  }
  return "Unknown";
}

bool PickColor(cv::Scalar &color)
{
#ifdef _WIN32
  static COLORREF custom_colors[16] = {};
  CHOOSECOLORA chooser = {};
  chooser.lStructSize = sizeof(chooser);
  chooser.rgbResult = RGB(
      static_cast<BYTE>(color[2]),
      static_cast<BYTE>(color[1]),
      static_cast<BYTE>(color[0]));
  chooser.lpCustColors = custom_colors;
  chooser.Flags = CC_FULLOPEN | CC_RGBINIT;

  if (!ChooseColorA(&chooser))
    return false;

  color = cv::Scalar(
      GetBValue(chooser.rgbResult),
      GetGValue(chooser.rgbResult),
      GetRValue(chooser.rgbResult));
  return true;
#else
  std::cerr << "The native color picker is currently implemented for Windows.\n";
  return false;
#endif
}

void DrawButton(cv::Mat &panel, const cv::Rect &rect,
                const std::string &label, const cv::Scalar &fill,
                bool selected = false)
{
  cv::rectangle(panel, rect, fill, cv::FILLED);
  cv::rectangle(panel, rect,
                selected ? cv::Scalar(0, 220, 255) : cv::Scalar(105, 105, 105),
                selected ? 3 : 1);

  const double luminance = 0.114 * fill[0] + 0.587 * fill[1] + 0.299 * fill[2];
  const cv::Scalar text_color =
      luminance > 145 ? cv::Scalar(20, 20, 20) : cv::Scalar(245, 245, 245);

  int baseline = 0;
  cv::Size text_size = cv::getTextSize(
      label, cv::FONT_HERSHEY_SIMPLEX, 0.55, 1, &baseline);
  cv::Point origin(
      rect.x + (rect.width - text_size.width) / 2,
      rect.y + (rect.height + text_size.height) / 2);
  cv::putText(panel, label, origin, cv::FONT_HERSHEY_SIMPLEX,
              0.55, text_color, 1, cv::LINE_AA);
}

void DrawControls()
{
  cv::Mat panel(235, 700, CV_8UC3, cv::Scalar(35, 35, 35));
  cv::putText(panel,
              "Arrow keys: left/right camera | 1-5: modes | Q/Esc: quit",
              cv::Point(20, 28), cv::FONT_HERSHEY_SIMPLEX,
              0.55, cv::Scalar(235, 235, 235), 1, cv::LINE_AA);

  DrawButton(panel, left_color_button, "Pick left color", left_anaglyph_color);
  DrawButton(panel, right_color_button, "Pick right color", right_anaglyph_color);

  const std::array<std::string, 5> labels = {
      "1 Left", "2 Right", "3 Side", "4 Blend", "5 Anaglyph"};
  for (std::size_t i = 0; i < mode_buttons.size(); ++i)
  {
    DrawButton(panel, mode_buttons[i], labels[i], cv::Scalar(65, 65, 65),
               static_cast<int>(display_mode) == static_cast<int>(i));
  }

  cv::putText(panel,
              std::string("Current mode: ") + DisplayModeName(display_mode),
              cv::Point(20, 222), cv::FONT_HERSHEY_SIMPLEX,
              0.52, cv::Scalar(220, 220, 220), 1, cv::LINE_AA);
  cv::imshow(controls_window, panel);
  controls_dirty = false;
}

void OnControlsMouse(int event, int x, int y, int, void *)
{
  if (event != cv::EVENT_LBUTTONDOWN)
    return;

  const cv::Point point(x, y);
  if (left_color_button.contains(point))
  {
    if (PickColor(left_anaglyph_color))
      controls_dirty = true;
    return;
  }
  if (right_color_button.contains(point))
  {
    if (PickColor(right_anaglyph_color))
      controls_dirty = true;
    return;
  }

  for (std::size_t i = 0; i < mode_buttons.size(); ++i)
  {
    if (mode_buttons[i].contains(point))
    {
      display_mode = static_cast<DisplayMode>(i);
      controls_dirty = true;
      return;
    }
  }
}

cv::Mat TintGrayImage(const cv::Mat &gray, const cv::Scalar &color)
{
  std::vector<cv::Mat> channels(3);
  gray.convertTo(channels[0], CV_8U, color[0] / 255.0);
  gray.convertTo(channels[1], CV_8U, color[1] / 255.0);
  gray.convertTo(channels[2], CV_8U, color[2] / 255.0);

  cv::Mat tinted;
  cv::merge(channels, tinted);
  return tinted;
}

cv::Mat MakeAnaglyph(const cv::Mat &left, const cv::Mat &right)
{
  cv::Mat left_gray, right_gray;
  cv::cvtColor(left, left_gray, cv::COLOR_BGR2GRAY);
  cv::cvtColor(right, right_gray, cv::COLOR_BGR2GRAY);

  cv::Mat left_tinted = TintGrayImage(left_gray, left_anaglyph_color);
  cv::Mat right_tinted = TintGrayImage(right_gray, right_anaglyph_color);
  cv::Mat result;
  cv::add(left_tinted, right_tinted, result);
  return result;
}

cv::Mat MakeViewerImage(const cv::Mat &left_raw, const cv::Mat &right_raw,
                        const cv::Mat &left_rectified,
                        const cv::Mat &right_rectified)
{
  cv::Mat result;
  switch (display_mode)
  {
  case DisplayMode::Left:
    left_raw.copyTo(result);
    break;
  case DisplayMode::Right:
    right_raw.copyTo(result);
    break;
  case DisplayMode::SideBySide:
    cv::hconcat(left_raw, right_raw, result);
    break;
  case DisplayMode::Blend:
    cv::addWeighted(left_raw, 0.5, right_raw, 0.5, 0.0, result);
    break;
  case DisplayMode::Anaglyph:
    result = MakeAnaglyph(left_rectified, right_rectified);
    break;
  }
  return result;
}

void HandleDisplayKey(int key)
{
  const int character = key & 0xff;

  // OpenCV/Win32 extended-key values for the left and right arrows.
  if (key == 2424832 || key == 65361)
    display_mode = DisplayMode::Left;
  else if (key == 2555904 || key == 65363)
    display_mode = DisplayMode::Right;
  else if (character >= '1' && character <= '5')
    display_mode = static_cast<DisplayMode>(character - '1');
  else if (character == 'a' || character == 'A')
    display_mode = DisplayMode::Anaglyph;
  else if (character == 'b' || character == 'B')
    display_mode = DisplayMode::Blend;
  else if (character == 's' || character == 'S')
    display_mode = DisplayMode::SideBySide;

  controls_dirty = true;
}

////////////////////////////////////////////////////////////////////////////////

void OnTrackAngle(int value, void *)
{
  vfov_bar = value;
  vfov_now = 60 + vfov_bar;
  changed = true;
}

////////////////////////////////////////////////////////////////////////////////

void OnTrackWidth(int value, void *)
{
  width_bar = value;
  width_now = 480 + width_bar;
  if (width_now % 2 == 1)
    width_now--;
  changed = true;
}

////////////////////////////////////////////////////////////////////////////////

void OnTrackHeight(int value, void *)
{
  height_bar = value;
  height_now = 360 + height_bar;
  if (height_now % 2 == 1)
    height_now--;
  changed = true;
}

////////////////////////////////////////////////////////////////////////////////

void OnTrackNdisp(int value, void *)
{
  ndisp_bar = value;
  ndisp_now = 16 + 16 * ndisp_bar;
  changed = true;
}

////////////////////////////////////////////////////////////////////////////////

void OnTrackWsize(int value, void *)
{
  wsize_bar = value;
  wsize_now = 3 + 2 * wsize_bar;
  changed = true;
}

////////////////////////////////////////////////////////////////////////////////

void LoadParameters(std::string file_name)
{
  cv::FileStorage fs(file_name, cv::FileStorage::READ);
  if (!fs.isOpened())
  {
    std::cout << "Failed to open ini parameters" << std::endl;
    exit(-1);
  }

  cv::Size cap_size;
  fs["cam_model"] >> cam_model;
  fs["cap_size"] >> cap_size;
  fs["Kl"] >> Kl;
  fs["Dl"] >> Dl;
  fs["xil"] >> xil;
  Rl = cv::Mat::eye(3, 3, CV_64F);
  if (cam_model == "stereo")
  {
    fs["Rl"] >> Rl;
    fs["Kr"] >> Kr;
    fs["Dr"] >> Dr;
    fs["xir"] >> xir;
    fs["Rr"] >> Rr;
    fs["T"] >> Translation;
  }
  fs.release();

  img_width = cap_size.width;
  cap_cols = cap_size.width;
  cap_rows = cap_size.height;

  if (cam_model == "stereo")
    img_width = cap_size.width / 2;
}

////////////////////////////////////////////////////////////////////////////////

inline double MatRowMul(cv::Mat m, double x, double y, double z, int r)
{
  return m.at<double>(r, 0) * x + m.at<double>(r, 1) * y + m.at<double>(r, 2) * z;
}

////////////////////////////////////////////////////////////////////////////////

void InitUndistortRectifyMap(cv::Mat K, cv::Mat D, cv::Mat xi, cv::Mat R,
                             cv::Mat P, cv::Size size,
                             cv::Mat &map1, cv::Mat &map2)
{
  map1 = cv::Mat(size, CV_32F);
  map2 = cv::Mat(size, CV_32F);

  double fx = K.at<double>(0, 0);
  double fy = K.at<double>(1, 1);
  double cx = K.at<double>(0, 2);
  double cy = K.at<double>(1, 2);
  double s = K.at<double>(0, 1);

  double xid = xi.at<double>(0, 0);

  double k1 = D.at<double>(0, 0);
  double k2 = D.at<double>(0, 1);
  double p1 = D.at<double>(0, 2);
  double p2 = D.at<double>(0, 3);

  cv::Mat KRi = (P * R).inv();

  for (int r = 0; r < size.height; ++r)
  {
    for (int c = 0; c < size.width; ++c)
    {
      double xc = MatRowMul(KRi, c, r, 1., 0);
      double yc = MatRowMul(KRi, c, r, 1., 1);
      double zc = MatRowMul(KRi, c, r, 1., 2);

      double rr = sqrt(xc * xc + yc * yc + zc * zc);
      double xs = xc / rr;
      double ys = yc / rr;
      double zs = zc / rr;

      double xu = xs / (zs + xid);
      double yu = ys / (zs + xid);

      double r2 = xu * xu + yu * yu;
      double r4 = r2 * r2;
      double xd = (1 + k1 * r2 + k2 * r4) * xu + 2 * p1 * xu * yu + p2 * (r2 + 2 * xu * xu);
      double yd = (1 + k1 * r2 + k2 * r4) * yu + 2 * p2 * xu * yu + p1 * (r2 + 2 * yu * yu);

      double u = fx * xd + s * yd + cx;
      double v = fy * yd + cy;

      map1.at<float>(r, c) = (float)u;
      map2.at<float>(r, c) = (float)v;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void InitRectifyMap()
{
  double vfov_rad = vfov_now * CV_PI / 180.;
  double focal = height_now / 2. / tan(vfov_rad / 2.);
  Knew = (cv::Mat_<double>(3, 3) << focal, 0., width_now / 2. - 0.5,
          0., focal, height_now / 2. - 0.5,
          0., 0., 1.);

  cv::Size img_size(width_now, height_now);

  InitUndistortRectifyMap(Kl, Dl, xil, Rl, Knew,
                          img_size, smap[0][0], smap[0][1]);

  std::cout << "Width: " << width_now << "\t"
            << "Height: " << height_now << "\t"
            << "V.Fov: " << vfov_now << "\n";
  std::cout << "K Matrix: \n"
            << Knew << std::endl;

  if (cam_model == "stereo")
  {
    InitUndistortRectifyMap(Kr, Dr, xir, Rr, Knew,
                            img_size, smap[1][0], smap[1][1]);
    std::cout << "Ndisp: " << ndisp_now << "\t"
              << "Wsize: " << wsize_now << "\n";
  }
  std::cout << std::endl;
}

////////////////////////////////////////////////////////////////////////////////

void DisparityImage(const cv::Mat &recl, const cv::Mat &recr, cv::Mat &disp)
{
  cv::Mat disp16s;
  int N = ndisp_now, W = wsize_now, C = recl.channels();
  if (is_sgbm)
  {
    cv::Ptr<cv::StereoSGBM> sgbm =
        cv::StereoSGBM::create(0, N, W, 8 * C * W * W, 32 * C * W * W);
    sgbm->compute(recl, recr, disp16s);
  }
  else
  {
    cv::Mat grayl, grayr;
    cv::cvtColor(recl, grayl, cv::COLOR_BGR2GRAY);
    cv::cvtColor(recr, grayr, cv::COLOR_BGR2GRAY);

    cv::Ptr<cv::StereoBM> sbm = cv::StereoBM::create(N, W);
    sbm->setPreFilterCap(31);
    sbm->setMinDisparity(0);
    sbm->setTextureThreshold(10);
    sbm->setUniquenessRatio(15);
    sbm->setSpeckleWindowSize(100);
    sbm->setSpeckleRange(32);
    sbm->setDisp12MaxDiff(1);
    sbm->compute(grayl, grayr, disp16s);
  }

  double minVal, maxVal;
  minMaxLoc(disp16s, &minVal, &maxVal);
  disp16s.convertTo(disp, CV_8UC1, 255 / (maxVal - minVal));

  /* How to get the depth map
  double fx = Knew.at<double>(0,0);
  double fy = Knew.at<double>(1,1);
  double cx = Knew.at<double>(0,2);
  double cy = Knew.at<double>(1,2);
  double bl = -Translation.at<double>(0,0);

  cv::Mat dispf;
  disp16s.convertTo(dispf, CV_32F, 1.f / 16.f);
  for (int r = 0; r < dispf.rows; ++r) {
    for (int c = 0; c < dispf.cols; ++c) {
      double e = (c - cx) / fx;
      double f = (r - cy) / fy;

      double disp  = dispf.at<float>(r,c);
      if (disp <= 0.f)
        continue;

      double depth = fx * bl / disp;
      double x = e * depth;
      double y = f * depth;
      double z = depth;
    }
  } */
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
  std::string file_name = argc >= 2 ? argv[1] : "../astar_calicam.yml";
  int camera_index = argc >= 3 ? std::stoi(argv[2]) : 0;
  LoadParameters(file_name);
  InitRectifyMap();

  cv::Mat raw_img;
  cv::VideoCapture vcapture;
  if (live)
  {
#ifdef _WIN32
    vcapture.open(camera_index, cv::CAP_DSHOW);
#else
    vcapture.open(camera_index, cv::CAP_V4L2);
#endif

    if (!vcapture.isOpened())
    {
      std::cerr << "Could not open camera index " << camera_index << std::endl;
      return -1;
    }

#ifdef _WIN32
    vcapture.set(cv::CAP_PROP_FOURCC,
                 cv::VideoWriter::fourcc('Y', 'U', 'Y', '2'));
#else
    vcapture.set(cv::CAP_PROP_FOURCC,
                 cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V'));
#endif
    vcapture.set(cv::CAP_PROP_FRAME_WIDTH, cap_cols);
    vcapture.set(cv::CAP_PROP_FRAME_HEIGHT, cap_rows);
    vcapture.set(cv::CAP_PROP_FPS, 30);

    std::cout << "Camera index: " << camera_index << '\n'
              << "Requested capture: " << cap_cols << " x " << cap_rows << '\n'
              << "Reported capture: "
              << vcapture.get(cv::CAP_PROP_FRAME_WIDTH) << " x "
              << vcapture.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;
  }
  else
  {
    raw_img = cv::imread("../dasl_wood_shop.jpg", cv::IMREAD_COLOR);
  }

  char win_name[256];
  sprintf(win_name, "Raw Image: %d x %d", img_width, cap_rows);
  std::string param_win_name(win_name);
  cv::namedWindow(param_win_name);

  cv::createTrackbar("V. FoV:  60    +", param_win_name, nullptr, vfov_max, OnTrackAngle);
  cv::createTrackbar("Width:  480 +", param_win_name, nullptr, width_max, OnTrackWidth);
  cv::createTrackbar("Height: 360 +", param_win_name, nullptr, height_max, OnTrackHeight);

  std::string disp_win_name = "Disparity Image";
  if (cam_model == "stereo")
  {
    cv::namedWindow(disp_win_name);
    cv::createTrackbar("Num Disp:  16 + 16 *", disp_win_name, nullptr, ndisp_max, OnTrackNdisp);
    cv::setTrackbarPos("Num Disp:  16 + 16 *", disp_win_name, ndisp_bar);
    cv::createTrackbar("Blk   Size :     3  +  2 *", disp_win_name, nullptr, wsize_max, OnTrackWsize);
    cv::setTrackbarPos("Blk   Size :     3  +  2 *", disp_win_name, wsize_bar);

    cv::namedWindow(viewer_window, cv::WINDOW_NORMAL);
    cv::namedWindow(anaglyph_window, cv::WINDOW_NORMAL);
    cv::namedWindow(controls_window, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(controls_window, OnControlsMouse);
    cv::resizeWindow(viewer_window, 1100, 520);
    cv::resizeWindow(anaglyph_window, 800, 600);
    cv::moveWindow(viewer_window, 20, 20);
    cv::moveWindow(anaglyph_window, 100, 100);
    cv::moveWindow(controls_window, 150, 150);
    DrawControls();
  }

  cv::Mat raw_imgl, raw_imgr, rect_imgl, rect_imgr;
  while (1)
  {
    if (changed)
    {
      InitRectifyMap();
      changed = false;
    }

    if (live)
      vcapture >> raw_img;

    if (raw_img.total() == 0)
    {
      std::cout << "Image capture error" << std::endl;
      exit(-1);
    }

    if (raw_img.cols != cap_cols || raw_img.rows != cap_rows)
    {
      std::cerr << "Incorrect frame size. Received "
                << raw_img.cols << " x " << raw_img.rows
                << ", expected " << cap_cols << " x " << cap_rows
                << ". Try a different camera index." << std::endl;
      return -1;
    }

    if (cam_model == "stereo")
    {
      raw_img(cv::Rect(0, 0, img_width, cap_rows)).copyTo(raw_imgl);
      raw_img(cv::Rect(img_width, 0, img_width, cap_rows)).copyTo(raw_imgr);

      cv::remap(raw_imgl, rect_imgl, smap[0][0], smap[0][1], 1, 0);
      cv::remap(raw_imgr, rect_imgr, smap[1][0], smap[1][1], 1, 0);
    }
    else
    {
      raw_imgl = raw_img;
      cv::remap(raw_img, rect_imgl, smap[0][0], smap[0][1], 1, 0);
    }

    cv::Mat small_img;
    cv::resize(raw_imgl, small_img, cv::Size(), 0.5, 0.5);
    imshow(param_win_name, small_img);
    imshow("Rectified Image", rect_imgl);

    if (cam_model == "stereo")
    {
      cv::Mat disp_img;
      DisparityImage(rect_imgl, rect_imgr, disp_img);
      imshow(disp_win_name, disp_img);

      cv::Mat anaglyph = MakeAnaglyph(rect_imgl, rect_imgr);
      cv::imshow(anaglyph_window, anaglyph);

      cv::Mat viewer = MakeViewerImage(raw_imgl, raw_imgr,
                                       rect_imgl, rect_imgr);
      cv::imshow(viewer_window, viewer);

      if (controls_dirty)
        DrawControls();
    }

    int key = cv::waitKeyEx(1);
    int character = key & 0xff;
    if (character == 'q' || character == 'Q' || character == 27)
      break;
    if (key != -1)
      HandleDisplayKey(key);
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
