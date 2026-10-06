
// This is No Warranty No Copyright Software.
// astar.ai
// May 15, 2019

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

////////////////////////////////////////////////////////////////////////////////

class PanoramaWindow {
 public:
  PanoramaWindow(int width, int height, const char* title) {
    if (!glfwInit()) {
      throw std::runtime_error("Could not initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
      glfwTerminate();
      throw std::runtime_error("Could not create the OpenGL window");
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    glfwSetWindowUserPointer(window_, this);
    glfwSetMouseButtonCallback(window_, MouseButtonCallback);
    glfwSetCursorPosCallback(window_, CursorCallback);
    glfwSetScrollCallback(window_, ScrollCallback);
    glfwSetKeyCallback(window_, KeyCallback);

    glEnable(GL_DEPTH_TEST);
    glPointSize(2.0f);
  }

  ~PanoramaWindow() {
    if (window_) {
      glfwMakeContextCurrent(window_);
      glDeleteTextures(2, inset_textures_);
      glfwDestroyWindow(window_);
    }
    glfwTerminate();
  }

  bool alive() const {
    return window_ && !glfwWindowShouldClose(window_);
  }

  bool start_draw() {
    if (!alive()) {
      return false;
    }

    glfwMakeContextCurrent(window_);
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    if (width <= 0 || height <= 0) {
      glfwPollEvents();
      return false;
    }

    glViewport(0, 0, width, height);
    glClearColor(0.035f, 0.035f, 0.045f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const double near_plane = 0.01;
    const double far_plane = 1000.0;
    const double half_height = near_plane * std::tan(45.0 * CV_PI / 360.0);
    const double half_width = half_height * static_cast<double>(width) /
                              static_cast<double>(height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-half_width, half_width, -half_height, half_height,
              near_plane, far_plane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(pan_x_, pan_y_, -distance_);
    glRotatef(pitch_, 1.0f, 0.0f, 0.0f);
    glRotatef(yaw_, 0.0f, 1.0f, 0.0f);
    return true;
  }

  void finish_draw() {
    glfwSwapBuffers(window_);
    glfwPollEvents();
  }

  void draw_image_inset(const cv::Mat& bgr_image, int slot,
                        int slot_count = 1) {
    if (bgr_image.empty() || slot < 0 || slot >= 2 ||
        slot >= slot_count || slot_count < 1 || slot_count > 2) {
      return;
    }

    cv::Mat scaled;
    const int target_width = slot_count == 1 ? 360 : 300;
    const double scale = static_cast<double>(target_width) /
                         static_cast<double>(bgr_image.cols);
    cv::resize(bgr_image, scaled, cv::Size(), scale, scale,
               cv::INTER_AREA);
    cv::Mat rgb_image;
    cv::cvtColor(scaled, rgb_image, cv::COLOR_BGR2RGB);

    if (inset_textures_[slot] == 0) {
      glGenTextures(1, &inset_textures_[slot]);
      glBindTexture(GL_TEXTURE_2D, inset_textures_[slot]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    } else {
      glBindTexture(GL_TEXTURE_2D, inset_textures_[slot]);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb_image.cols, rgb_image.rows,
                 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_image.data);

    int viewport_width = 0;
    int viewport_height = 0;
    glfwGetFramebufferSize(window_, &viewport_width, &viewport_height);
    if (viewport_width <= 0 || viewport_height <= 0) {
      return;
    }

    const float inset_width = slot_count == 1 ? 0.38f : 0.31f;
    const float inset_height = inset_width *
        (static_cast<float>(rgb_image.rows) / rgb_image.cols) *
        (static_cast<float>(viewport_width) / viewport_height);
    const float x0 = 0.025f + slot * (inset_width + 0.018f);
    const float y0 = 0.025f;
    const float x1 = x0 + inset_width;
    const float y1 = y0 + inset_height;

    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_TEXTURE_2D);
    glColor3ub(20, 220, 80);
    glBegin(GL_QUADS);
    glVertex2f(x0 - 0.006f, y0 - 0.006f);
    glVertex2f(x1 + 0.006f, y0 - 0.006f);
    glVertex2f(x1 + 0.006f, y1 + 0.006f);
    glVertex2f(x0 - 0.006f, y1 + 0.006f);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glColor3ub(255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y1);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y1);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
  }

 private:
  static PanoramaWindow* From(GLFWwindow* window) {
    return static_cast<PanoramaWindow*>(glfwGetWindowUserPointer(window));
  }

  static void MouseButtonCallback(GLFWwindow* window, int button,
                                  int action, int) {
    PanoramaWindow* self = From(window);
    if (!self) {
      return;
    }

    const bool pressed = action == GLFW_PRESS;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
      self->rotating_ = pressed;
    } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
      self->panning_ = pressed;
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
      self->zooming_ = pressed;
    }
    glfwGetCursorPos(window, &self->last_x_, &self->last_y_);
  }

  static void CursorCallback(GLFWwindow* window, double x, double y) {
    PanoramaWindow* self = From(window);
    if (!self) {
      return;
    }

    const double dx = x - self->last_x_;
    const double dy = y - self->last_y_;
    self->last_x_ = x;
    self->last_y_ = y;

    if (self->rotating_) {
      self->yaw_ += static_cast<float>(dx * 0.35);
      self->pitch_ += static_cast<float>(dy * 0.35);
    }
    if (self->panning_) {
      const float scale = std::max(0.001f, self->distance_ * 0.0015f);
      self->pan_x_ += static_cast<float>(dx) * scale;
      self->pan_y_ -= static_cast<float>(dy) * scale;
    }
    if (self->zooming_) {
      self->distance_ *= static_cast<float>(std::exp(dy * 0.01));
      self->ClampDistance();
    }
  }

  static void ScrollCallback(GLFWwindow* window, double, double y_offset) {
    PanoramaWindow* self = From(window);
    if (!self) {
      return;
    }
    self->distance_ *= static_cast<float>(std::exp(-y_offset * 0.12));
    self->ClampDistance();
  }

  static void KeyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) {
      return;
    }
    PanoramaWindow* self = From(window);
    if (!self) {
      return;
    }
    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (key == GLFW_KEY_R) {
      self->yaw_ = 0.0f;
      self->pitch_ = 0.0f;
      self->pan_x_ = 0.0f;
      self->pan_y_ = 0.0f;
      self->distance_ = 2.5f;
    }
  }

  void ClampDistance() {
    distance_ = std::max(0.02f, std::min(distance_, 1000.0f));
  }

  GLFWwindow* window_ = nullptr;
  bool rotating_ = false;
  bool panning_ = false;
  bool zooming_ = false;
  double last_x_ = 0.0;
  double last_y_ = 0.0;
  float yaw_ = 0.0f;
  float pitch_ = 0.0f;
  float pan_x_ = 0.0f;
  float pan_y_ = 0.0f;
  float distance_ = 2.5f;
  GLuint inset_textures_[2] = {0, 0};
};

////////////////////////////////////////////////////////////////////////////////

int ndisp_bar = 1, wsize_bar = 2, thr_bar = 0;
int ndisp_max = 2, wsize_max = 3, thr_max = 30;
int ndisp_now = 32, wsize_now = 9, thr_now = 70;

////////////////////////////////////////////////////////////////////////////////

void OnTrackNdisp(int value, void*) {
  ndisp_bar = value;
  ndisp_now = 16 + 16 * value;
}

////////////////////////////////////////////////////////////////////////////////

void OnTrackWsize(int value, void*) {
  wsize_bar = value;
  wsize_now = 5 + 2 * value;
}

////////////////////////////////////////////////////////////////////////////////

void OnTrackThreshold(int value, void*) {
  thr_bar = value;
  thr_now = 70 + value;
}

////////////////////////////////////////////////////////////////////////////////

int cap_cols = 0, cap_rows = 0, img_width = 0;
cv::Mat T, Kl, Kr, Dl, Dr, xil, xir, Rl, Rr;
cv::Mat lmap[2][2];

// CaliCam stereo is calibrated at 2560x960
// If you want to process image at 1280x480, set half_size to true.

bool half_size = false;

void LoadParameters(const std::string& file_name) {
  cv::FileStorage fs(file_name, cv::FileStorage::READ);
  if (!fs.isOpened()) {
    throw std::runtime_error("Failed to open calibration parameters: " + file_name);
  }

  cv::Size cap_size;
  fs["cap_size" ] >> cap_size;
  fs["Kl"       ] >> Kl;
  fs["Dl"       ] >> Dl;
  fs["xil"      ] >> xil;
  fs["Rl"       ] >> Rl;
  fs["Kr"       ] >> Kr;
  fs["Dr"       ] >> Dr;
  fs["xir"      ] >> xir;
  fs["Rr"       ] >> Rr;
  fs["T"        ] >> T;
  fs.release();

  if (cap_size.width <= 0 || cap_size.height <= 0 ||
      cap_size.width % 2 != 0) {
    throw std::runtime_error("Invalid stereo capture size in calibration file");
  }

  if (half_size) {
    cap_size  = cap_size / 2;
    Kl.row(0) = Kl.row(0) / 2.;
    Kl.row(1) = Kl.row(1) / 2.;
    Kr.row(0) = Kr.row(0) / 2.;
    Kr.row(1) = Kr.row(1) / 2.;
  }

  cap_cols  = cap_size.width;
  cap_rows  = cap_size.height;
  img_width = cap_size.width / 2;
}

////////////////////////////////////////////////////////////////////////////////

inline double MatRowMul(const cv::Mat& m, double x, double y, double z, int r) {
  return m.at<double>(r,0) * x + m.at<double>(r,1) * y + m.at<double>(r,2) * z;
}

////////////////////////////////////////////////////////////////////////////////

enum RectMode {
  RECT_PERSPECTIVE,
  RECT_FISHEYE,
  RECT_LONGLAT
};

void InitRectifyMap(const cv::Mat& K,
                    const cv::Mat& D,
                    const cv::Mat& R,
                    const cv::Mat& Knew,
                    double xi0,
                    cv::Size size,
                    RectMode mode,
                    cv::Mat& map1,
                    cv::Mat& map2) {
  map1.create(size, CV_32F);
  map2.create(size, CV_32F);

  double fx = K.at<double>(0,0);
  double fy = K.at<double>(1,1);
  double cx = K.at<double>(0,2);
  double cy = K.at<double>(1,2);
  double s  = K.at<double>(0,1);

  double k1 = D.at<double>(0,0);
  double k2 = D.at<double>(0,1);
  double p1 = D.at<double>(0,2);
  double p2 = D.at<double>(0,3);

  cv::Mat Ki  = Knew.inv();
  cv::Mat Ri  = R.inv();
  cv::Mat KRi = (Knew * R).inv();

  for (int r = 0; r < size.height; ++r) {
    for (int c = 0; c < size.width; ++c) {
      double xc = 0.;
      double yc = 0.;
      double zc = 0.;

      if (mode == RECT_PERSPECTIVE) {
        xc = MatRowMul(KRi, c, r, 1., 0);
        yc = MatRowMul(KRi, c, r, 1., 1);
        zc = MatRowMul(KRi, c, r, 1., 2);
      }

      if (mode == RECT_LONGLAT) {
        double tt = MatRowMul(Ki, c, r, 1., 0);
        double pp = MatRowMul(Ki, c, r, 1., 1);

        double xn = -std::cos(tt);
        double yn = -std::sin(tt) * std::cos(pp);
        double zn =  std::sin(tt) * std::sin(pp);

        xc = MatRowMul(Ri, xn, yn, zn, 0);
        yc = MatRowMul(Ri, xn, yn, zn, 1);
        zc = MatRowMul(Ri, xn, yn, zn, 2);
      }

      if (mode == RECT_FISHEYE) {
        double ee = MatRowMul(Ki, c, r, 1., 0);
        double ff = MatRowMul(Ki, c, r, 1., 1);
        double zz = 2. / (ee * ee + ff * ff + 1.);

        double xn = zz * ee;
        double yn = zz * ff;
        double zn = zz - 1.;

        xc = MatRowMul(Ri, xn, yn, zn, 0);
        yc = MatRowMul(Ri, xn, yn, zn, 1);
        zc = MatRowMul(Ri, xn, yn, zn, 2);
      }

      double rr = std::sqrt(xc * xc + yc * yc + zc * zc);
      double xs = xc / rr;
      double ys = yc / rr;
      double zs = zc / rr;

      double xu = xs / (zs + xi0);
      double yu = ys / (zs + xi0);

      double r2 = xu * xu + yu * yu;
      double r4 = r2 * r2;
      double xd = (1+k1*r2+k2*r4)*xu + 2*p1*xu*yu + p2*(r2+2*xu*xu);
      double yd = (1+k1*r2+k2*r4)*yu + 2*p2*xu*yu + p1*(r2+2*yu*yu);

      double u = fx * xd + s * yd + cx;
      double v = fy * yd + cy;

      map1.at<float>(r,c) = (float) u;
      map2.at<float>(r,c) = (float) v;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

int rect_cols = 640, rect_rows = 640;
cv::Mat direction_map;
cv::Mat view_angle_map;
std::vector<float> column_phase;

void InitRectifyMap() {
  cv::Size img_size(rect_cols, rect_rows);
  cv::Mat  Kll = cv::Mat::eye(3, 3, CV_64F);
  Kll.at<double>(0,0) = (img_size.width - 1.) / CV_PI;
  Kll.at<double>(1,1) = (img_size.height - 1.) / CV_PI;

  InitRectifyMap(Kl, Dl, Rl, Kll, xil.at<double>(0,0),
                 img_size, RECT_LONGLAT, lmap[0][0], lmap[0][1]);
  InitRectifyMap(Kr, Dr, Rr, Kll, xir.at<double>(0,0),
                 img_size, RECT_LONGLAT, lmap[1][0], lmap[1][1]);
}

void InitPointCloudLookup() {
  direction_map.create(rect_rows, rect_cols, CV_32FC3);
  view_angle_map.create(rect_rows, rect_cols, CV_32F);
  column_phase.resize(rect_cols);

  for (int c = 0; c < rect_cols; ++c) {
    column_phase[c] = static_cast<float>(
        c * CV_PI / static_cast<double>(rect_cols - 1));
  }

  for (int r = 0; r < rect_rows; ++r) {
    const double pp =
        (r / static_cast<double>(rect_rows - 1) - 0.5) * CV_PI;
    for (int c = 0; c < rect_cols; ++c) {
      const double tt =
          (c / static_cast<double>(rect_cols - 1) - 0.5) * CV_PI;

      const float cx = static_cast<float>(std::sin(tt));
      const float cy = static_cast<float>(std::cos(tt) * std::sin(pp));
      const float cz = static_cast<float>(std::cos(tt) * std::cos(pp));
      direction_map.at<cv::Vec3f>(r, c) = cv::Vec3f(cx, cy, cz);
      view_angle_map.at<float>(r, c) = static_cast<float>(
          std::acos(std::max(-1.0f, std::min(cz, 1.0f))));
    }
  }
}

struct CalibrationContext {
  int capture_columns = 0;
  int capture_rows = 0;
  int single_image_width = 0;
  cv::Mat translation;
  cv::Mat left_intrinsics;
  cv::Mat right_intrinsics;
  cv::Mat left_distortion;
  cv::Mat right_distortion;
  cv::Mat left_xi;
  cv::Mat right_xi;
  cv::Mat left_rotation;
  cv::Mat right_rotation;
  cv::Mat rectify_map[2][2];
};

CalibrationContext LoadCalibrationContext(const std::string& filename) {
  LoadParameters(filename);
  InitRectifyMap();

  CalibrationContext context;
  context.capture_columns = cap_cols;
  context.capture_rows = cap_rows;
  context.single_image_width = img_width;
  context.translation = T.clone();
  context.left_intrinsics = Kl.clone();
  context.right_intrinsics = Kr.clone();
  context.left_distortion = Dl.clone();
  context.right_distortion = Dr.clone();
  context.left_xi = xil.clone();
  context.right_xi = xir.clone();
  context.left_rotation = Rl.clone();
  context.right_rotation = Rr.clone();
  for (int camera = 0; camera < 2; ++camera) {
    for (int coordinate = 0; coordinate < 2; ++coordinate) {
      context.rectify_map[camera][coordinate] =
          lmap[camera][coordinate].clone();
    }
  }
  return context;
}

void ApplyCalibrationContext(const CalibrationContext& context) {
  cap_cols = context.capture_columns;
  cap_rows = context.capture_rows;
  img_width = context.single_image_width;
  T = context.translation;
  Kl = context.left_intrinsics;
  Kr = context.right_intrinsics;
  Dl = context.left_distortion;
  Dr = context.right_distortion;
  xil = context.left_xi;
  xir = context.right_xi;
  Rl = context.left_rotation;
  Rr = context.right_rotation;
  for (int camera = 0; camera < 2; ++camera) {
    for (int coordinate = 0; coordinate < 2; ++coordinate) {
      lmap[camera][coordinate] = context.rectify_map[camera][coordinate];
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

cv::Ptr<cv::StereoSGBM> stereo_matcher;
int matcher_disparities = 0;
int matcher_window = 0;

void DisparityImage(const cv::Mat& recl, const cv::Mat& recr, cv::Mat& dispf) {
  cv::Mat grayl, grayr;
  cv::cvtColor(recl, grayl, cv::COLOR_BGR2GRAY);
  cv::cvtColor(recr, grayr, cv::COLOR_BGR2GRAY);

  if (!stereo_matcher || matcher_disparities != ndisp_now ||
      matcher_window != wsize_now) {
    const int penalty1 = 8 * wsize_now * wsize_now;
    const int penalty2 = 32 * wsize_now * wsize_now;
    stereo_matcher = cv::StereoSGBM::create(
        0, ndisp_now, wsize_now, penalty1, penalty2);
    matcher_disparities = ndisp_now;
    matcher_window = wsize_now;
  }

  cv::Mat disps;
  stereo_matcher->compute(grayl, grayr, disps);
  disps.convertTo(dispf, CV_32F, 1.0 / 16.0);
}

////////////////////////////////////////////////////////////////////////////////

struct PCL {
  cv::Vec3f pts;
  cv::Vec3b clr;
};

struct VoxelKey {
  int x = 0;
  int y = 0;
  int z = 0;

  bool operator==(const VoxelKey& other) const {
    return x == other.x && y == other.y && z == other.z;
  }
};

struct VoxelKeyHash {
  std::size_t operator()(const VoxelKey& key) const {
    std::size_t seed = std::hash<int>()(key.x);
    seed ^= std::hash<int>()(key.y) + 0x9e3779b9u +
            (seed << 6) + (seed >> 2);
    seed ^= std::hash<int>()(key.z) + 0x9e3779b9u +
            (seed << 6) + (seed >> 2);
    return seed;
  }
};

struct VoxelCell {
  cv::Vec3f average_position = cv::Vec3f(0.0f, 0.0f, 0.0f);
  cv::Vec3f average_color = cv::Vec3f(0.0f, 0.0f, 0.0f);
  uint32_t observations = 0;
};

class VoxelPointMap {
 public:
  explicit VoxelPointMap(float voxel_size_meters)
      : voxel_size_(voxel_size_meters) {
    if (!(voxel_size_ > 0.0f)) {
      throw std::runtime_error("Voxel size must be positive");
    }
    voxels_.reserve(300000);
  }

  void Clear() {
    voxels_.clear();
    render_cache_.clear();
    observation_count_ = 0;
    cache_dirty_ = false;
  }

  void Insert(const PCL& point) {
    if (!std::isfinite(point.pts[0]) || !std::isfinite(point.pts[1]) ||
        !std::isfinite(point.pts[2])) {
      return;
    }

    const VoxelKey key = {
        static_cast<int>(std::floor(point.pts[0] / voxel_size_)),
        static_cast<int>(std::floor(point.pts[1] / voxel_size_)),
        static_cast<int>(std::floor(point.pts[2] / voxel_size_))};
    VoxelCell& cell = voxels_[key];
    const cv::Vec3f color(static_cast<float>(point.clr[0]),
                          static_cast<float>(point.clr[1]),
                          static_cast<float>(point.clr[2]));
    if (cell.observations == 0) {
      cell.average_position = point.pts;
      cell.average_color = color;
      cell.observations = 1;
    } else {
      if (cell.observations < std::numeric_limits<uint32_t>::max()) {
        ++cell.observations;
      }
      const float weight = 1.0f / static_cast<float>(cell.observations);
      cell.average_position += (point.pts - cell.average_position) * weight;
      cell.average_color += (color - cell.average_color) * weight;
    }
    ++observation_count_;
    cache_dirty_ = true;
  }

  const std::vector<PCL>& Points() const {
    if (!cache_dirty_) {
      return render_cache_;
    }

    render_cache_.clear();
    render_cache_.reserve(voxels_.size());
    for (const auto& entry : voxels_) {
      const VoxelCell& cell = entry.second;
      if (cell.observations == 0) {
        continue;
      }
      PCL point;
      point.pts = cell.average_position;
      point.clr = cv::Vec3b(
          cv::saturate_cast<unsigned char>(cell.average_color[0]),
          cv::saturate_cast<unsigned char>(cell.average_color[1]),
          cv::saturate_cast<unsigned char>(cell.average_color[2]));
      render_cache_.push_back(point);
    }
    cache_dirty_ = false;
    return render_cache_;
  }

  std::size_t voxel_count() const { return voxels_.size(); }
  uint64_t observation_count() const { return observation_count_; }
  float voxel_size() const { return voxel_size_; }

 private:
  float voxel_size_ = 0.04f;
  std::unordered_map<VoxelKey, VoxelCell, VoxelKeyHash> voxels_;
  mutable std::vector<PCL> render_cache_;
  uint64_t observation_count_ = 0;
  mutable bool cache_dirty_ = false;
};

constexpr float kMapVoxelSizeMeters = 0.04f;

constexpr float kMinimumMapDepth = 0.08f;
constexpr float kMaximumMapDepth = 30.0f;

bool PointAtPixel(const cv::Mat& disp_img, int c, int r, cv::Vec3f& point) {
  if (c < 0 || c >= disp_img.cols || r < 0 || r >= disp_img.rows) {
    return false;
  }

  const float disp = disp_img.at<float>(r, c);
  if (disp <= 0.0f || view_angle_map.at<float>(r, c) >
                          static_cast<float>((CV_PI / 2.0) *
                                             (thr_now / 100.0))) {
    return false;
  }

  const double pi_w = CV_PI / (rect_cols - 1.0);
  const double diff = pi_w * disp;
  const double denominator = std::sin(diff);
  if (std::abs(denominator) < 1e-8) {
    return false;
  }

  const double magnitude =
      cv::norm(T) * std::sin(column_phase[c] - diff) / denominator;
  if (!std::isfinite(magnitude) || magnitude < kMinimumMapDepth ||
      magnitude > kMaximumMapDepth) {
    return false;
  }

  point = direction_map.at<cv::Vec3f>(r, c) *
          static_cast<float>(magnitude);
  return true;
}

void PointClouds(const cv::Mat& disp_img,
                 const cv::Mat& color_img,
                 std::vector<PCL>& pcl_vec) {
  pcl_vec.clear();
  pcl_vec.reserve(color_img.total());

  for (int r = 0; r < color_img.rows; ++r) {
    for (int c = 0; c < color_img.cols; ++c) {
      cv::Vec3f point;
      if (!PointAtPixel(disp_img, c, r, point)) {
        continue;
      }

      const cv::Vec3b color = color_img.at<cv::Vec3b>(r, c);

      PCL pcl;
      pcl.pts = point;
      pcl.clr = cv::Vec3b(color(2), color(1), color(0));
      pcl_vec.push_back(pcl);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void DrawScene(const std::vector<PCL>& pcl_vec) {
  if (pcl_vec.empty()) {
    return;
  }

  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glVertexPointer(3, GL_FLOAT, sizeof(PCL), &pcl_vec.front().pts[0]);
  glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(PCL), &pcl_vec.front().clr[0]);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(pcl_vec.size()));
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
}

void DrawMonochromeMap(const std::vector<PCL>& pcl_vec) {
  if (pcl_vec.empty()) {
    return;
  }

  glColor3ub(220, 225, 220);
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(3, GL_FLOAT, sizeof(PCL), &pcl_vec.front().pts[0]);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(pcl_vec.size()));
  glDisableClientState(GL_VERTEX_ARRAY);
}

////////////////////////////////////////////////////////////////////////////////

struct Pose3f {
  cv::Matx33f rotation = cv::Matx33f::eye();
  cv::Vec3f translation = cv::Vec3f(0.0f, 0.0f, 0.0f);
};

cv::Vec3f TransformPoint(const Pose3f& pose, const cv::Vec3f& point) {
  return pose.rotation * point + pose.translation;
}

cv::Vec3d ToVec3d(const cv::Vec3f& value) {
  return cv::Vec3d(static_cast<double>(value[0]),
                   static_cast<double>(value[1]),
                   static_cast<double>(value[2]));
}

float RotationAngleDegrees(const cv::Matx33f& rotation) {
  const float cosine = std::max(
      -1.0f, std::min(1.0f, (rotation(0, 0) + rotation(1, 1) +
                             rotation(2, 2) - 1.0f) * 0.5f));
  return static_cast<float>(std::acos(cosine) * 180.0 / CV_PI);
}

bool EstimateRigidSvd(const std::vector<cv::Vec3f>& source,
                      const std::vector<cv::Vec3f>& destination,
                      const std::vector<int>& indices,
                      cv::Matx33f& rotation,
                      cv::Vec3f& translation) {
  if (indices.size() < 3) {
    return false;
  }

  cv::Vec3d source_center(0.0, 0.0, 0.0);
  cv::Vec3d destination_center(0.0, 0.0, 0.0);
  for (int index : indices) {
    source_center += ToVec3d(source[index]);
    destination_center += ToVec3d(destination[index]);
  }
  source_center *= 1.0 / static_cast<double>(indices.size());
  destination_center *= 1.0 / static_cast<double>(indices.size());

  cv::Mat covariance = cv::Mat::zeros(3, 3, CV_64F);
  for (int index : indices) {
    const cv::Vec3d source_delta =
        ToVec3d(source[index]) - source_center;
    const cv::Vec3d destination_delta =
        ToVec3d(destination[index]) - destination_center;
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) {
        covariance.at<double>(row, col) +=
            source_delta[row] * destination_delta[col];
      }
    }
  }

  cv::SVD svd(covariance, cv::SVD::FULL_UV);
  cv::Mat rotation_matrix = svd.vt.t() * svd.u.t();
  if (cv::determinant(rotation_matrix) < 0.0) {
    cv::Mat corrected_v = svd.vt.t();
    corrected_v.col(2) *= -1.0;
    rotation_matrix = corrected_v * svd.u.t();
  }

  cv::Matx33d rotation_double;
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      rotation_double(row, col) = rotation_matrix.at<double>(row, col);
      rotation(row, col) = static_cast<float>(rotation_double(row, col));
    }
  }

  const cv::Vec3d translation_double =
      destination_center - rotation_double * source_center;
  translation = cv::Vec3f(static_cast<float>(translation_double[0]),
                          static_cast<float>(translation_double[1]),
                          static_cast<float>(translation_double[2]));
  return cv::checkRange(rotation_matrix) &&
         std::isfinite(cv::norm(translation));
}

struct MotionEstimate {
  bool valid = false;
  cv::Matx33f rotation = cv::Matx33f::eye();
  cv::Vec3f translation = cv::Vec3f(0.0f, 0.0f, 0.0f);
  int inliers = 0;
  int correspondences = 0;
};

MotionEstimate EstimateRigidRansac(
    const std::vector<cv::Vec3f>& source,
    const std::vector<cv::Vec3f>& destination) {
  MotionEstimate result;
  result.correspondences = static_cast<int>(source.size());
  if (source.size() < 12 || source.size() != destination.size()) {
    return result;
  }

  constexpr int kIterations = 240;
  constexpr float kInlierDistance = 0.12f;
  cv::RNG random(static_cast<uint64_t>(cv::getTickCount()));
  std::vector<int> best_inliers;

  for (int iteration = 0; iteration < kIterations; ++iteration) {
    std::vector<int> sample;
    while (sample.size() < 3) {
      const int candidate = random.uniform(0, static_cast<int>(source.size()));
      if (std::find(sample.begin(), sample.end(), candidate) == sample.end()) {
        sample.push_back(candidate);
      }
    }

    const cv::Vec3f source_cross =
        (source[sample[1]] - source[sample[0]]).cross(
            source[sample[2]] - source[sample[0]]);
    const cv::Vec3f destination_cross =
        (destination[sample[1]] - destination[sample[0]]).cross(
            destination[sample[2]] - destination[sample[0]]);
    if (cv::norm(source_cross) < 1e-4 ||
        cv::norm(destination_cross) < 1e-4) {
      continue;
    }

    cv::Matx33f rotation;
    cv::Vec3f translation;
    if (!EstimateRigidSvd(source, destination, sample,
                          rotation, translation)) {
      continue;
    }

    std::vector<int> inliers;
    inliers.reserve(source.size());
    for (int index = 0; index < static_cast<int>(source.size()); ++index) {
      if (cv::norm(rotation * source[index] + translation -
                   destination[index]) < kInlierDistance) {
        inliers.push_back(index);
      }
    }
    if (inliers.size() > best_inliers.size()) {
      best_inliers.swap(inliers);
    }
  }

  const int minimum_inliers =
      std::max(10, static_cast<int>(source.size() * 0.25));
  if (static_cast<int>(best_inliers.size()) < minimum_inliers) {
    return result;
  }

  if (!EstimateRigidSvd(source, destination, best_inliers,
                        result.rotation, result.translation)) {
    return result;
  }

  result.inliers = static_cast<int>(best_inliers.size());
  result.valid = true;
  return result;
}

class TemporalMapper {
 public:
  TemporalMapper()
      : orb_(cv::ORB::create(1600, 1.2f, 8, 31, 0, 2,
                             cv::ORB::HARRIS_SCORE, 31, 12)) {}

  void Reset() {
    initialized_ = false;
    tracking_ok_ = false;
    status_ = "waiting for first mapping frame";
    previous_keypoints_.clear();
    previous_descriptors_.release();
    previous_disparity_.release();
    world_pose_ = Pose3f();
    last_keyframe_pose_ = Pose3f();
    voxel_map_.Clear();
    trajectory_.clear();
    keyframe_poses_.clear();
  }

  void ProcessFrame(const cv::Mat& color_image,
                    const cv::Mat& disparity,
                    const std::vector<PCL>& local_cloud) {
    cv::Mat gray;
    cv::cvtColor(color_image, gray, cv::COLOR_BGR2GRAY);

    cv::Mat edge_mask;
    cv::Canny(gray, edge_mask, 50.0, 120.0);
    cv::dilate(edge_mask, edge_mask,
               cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5)));

    std::vector<cv::KeyPoint> current_keypoints;
    cv::Mat current_descriptors;
    orb_->detectAndCompute(gray, edge_mask, current_keypoints,
                           current_descriptors);

    if (current_descriptors.empty()) {
      tracking_ok_ = false;
      status_ = "tracking lost: no edge features";
      return;
    }

    if (!initialized_) {
      initialized_ = true;
      tracking_ok_ = true;
      world_pose_ = Pose3f();
      last_keyframe_pose_ = world_pose_;
      AddKeyframe(local_cloud);
      trajectory_.push_back(world_pose_.translation);
      keyframe_poses_.push_back(world_pose_);
      SavePrevious(current_keypoints, current_descriptors, disparity);
      status_ = "map initialized";
      return;
    }

    std::vector<std::vector<cv::DMatch>> knn_matches;
    cv::BFMatcher(cv::NORM_HAMMING).knnMatch(
        current_descriptors, previous_descriptors_, knn_matches, 2);

    std::vector<cv::Vec3f> current_points;
    std::vector<cv::Vec3f> previous_points;
    current_points.reserve(knn_matches.size());
    previous_points.reserve(knn_matches.size());

    for (const std::vector<cv::DMatch>& candidates : knn_matches) {
      if (candidates.size() < 2 ||
          candidates[0].distance >= 0.75f * candidates[1].distance ||
          candidates[0].distance > 64.0f) {
        continue;
      }

      const cv::Point2f current_pixel =
          current_keypoints[candidates[0].queryIdx].pt;
      const cv::Point2f previous_pixel =
          previous_keypoints_[candidates[0].trainIdx].pt;
      cv::Vec3f current_point;
      cv::Vec3f previous_point;
      if (PointAtPixel(disparity, cvRound(current_pixel.x),
                       cvRound(current_pixel.y), current_point) &&
          PointAtPixel(previous_disparity_, cvRound(previous_pixel.x),
                       cvRound(previous_pixel.y), previous_point)) {
        current_points.push_back(current_point);
        previous_points.push_back(previous_point);
      }
    }

    MotionEstimate motion =
        EstimateRigidRansac(current_points, previous_points);
    if (!motion.valid) {
      tracking_ok_ = false;
      status_ = "tracking lost: " +
                std::to_string(motion.correspondences) +
                " usable stereo matches";
      return;
    }

    const float frame_translation = cv::norm(motion.translation);
    const float frame_rotation = RotationAngleDegrees(motion.rotation);
    if (frame_translation > 1.5f || frame_rotation > 45.0f) {
      tracking_ok_ = false;
      status_ = "rejected implausible camera motion";
      return;
    }

    const Pose3f previous_world_pose = world_pose_;
    world_pose_.rotation = previous_world_pose.rotation * motion.rotation;
    world_pose_.translation =
        previous_world_pose.rotation * motion.translation +
        previous_world_pose.translation;
    tracking_ok_ = true;
    trajectory_.push_back(world_pose_.translation);

    const float keyframe_translation =
        cv::norm(world_pose_.translation - last_keyframe_pose_.translation);
    const cv::Matx33f keyframe_rotation =
        last_keyframe_pose_.rotation.t() * world_pose_.rotation;
    const float keyframe_angle = RotationAngleDegrees(keyframe_rotation);
    if (keyframe_translation >= 0.08f || keyframe_angle >= 5.0f) {
      AddKeyframe(local_cloud);
      last_keyframe_pose_ = world_pose_;
      keyframe_poses_.push_back(world_pose_);
    }

    status_ = "tracking: " + std::to_string(motion.inliers) + "/" +
              std::to_string(motion.correspondences) + " 3D inliers | " +
              std::to_string(voxel_map_.voxel_count()) + " voxels";
    SavePrevious(current_keypoints, current_descriptors, disparity);
  }

  bool SavePly(const std::string& filename) const {
    const std::vector<PCL>& map_points = voxel_map_.Points();
    if (map_points.empty()) {
      return false;
    }

    std::ofstream output(filename, std::ios::out | std::ios::trunc);
    if (!output) {
      return false;
    }
    output << "ply\nformat ascii 1.0\n"
           << "element vertex " << map_points.size() << "\n"
           << "property float x\nproperty float y\nproperty float z\n"
           << "property uchar red\nproperty uchar green\n"
           << "property uchar blue\nend_header\n";
    output << std::fixed << std::setprecision(5);
    for (const PCL& point : map_points) {
      output << point.pts[0] << ' ' << point.pts[1] << ' '
             << point.pts[2] << ' ' << static_cast<int>(point.clr[0])
             << ' ' << static_cast<int>(point.clr[1]) << ' '
             << static_cast<int>(point.clr[2]) << '\n';
    }
    return static_cast<bool>(output);
  }

  const std::vector<PCL>& map_points() const { return voxel_map_.Points(); }
  std::size_t voxel_count() const { return voxel_map_.voxel_count(); }
  const std::vector<cv::Vec3f>& trajectory() const { return trajectory_; }
  const std::vector<Pose3f>& keyframe_poses() const { return keyframe_poses_; }
  const Pose3f& world_pose() const { return world_pose_; }
  const std::string& status() const { return status_; }
  bool tracking_ok() const { return tracking_ok_; }

 private:
  void SavePrevious(const std::vector<cv::KeyPoint>& keypoints,
                    const cv::Mat& descriptors,
                    const cv::Mat& disparity) {
    previous_keypoints_ = keypoints;
    descriptors.copyTo(previous_descriptors_);
    disparity.copyTo(previous_disparity_);
  }

  void AddKeyframe(const std::vector<PCL>& local_cloud) {
    constexpr std::size_t kPointStride = 32;
    for (std::size_t index = 0; index < local_cloud.size();
         index += kPointStride) {
      PCL world_point = local_cloud[index];
      world_point.pts = TransformPoint(world_pose_, world_point.pts);
      voxel_map_.Insert(world_point);
    }
  }

  cv::Ptr<cv::ORB> orb_;
  bool initialized_ = false;
  bool tracking_ok_ = false;
  std::string status_ = "mapping disabled";
  std::vector<cv::KeyPoint> previous_keypoints_;
  cv::Mat previous_descriptors_;
  cv::Mat previous_disparity_;
  Pose3f world_pose_;
  Pose3f last_keyframe_pose_;
  VoxelPointMap voxel_map_{kMapVoxelSizeMeters};
  std::vector<cv::Vec3f> trajectory_;
  std::vector<Pose3f> keyframe_poses_;
};

void DrawTrajectory(const std::vector<cv::Vec3f>& trajectory) {
  if (trajectory.empty()) {
    return;
  }

  glLineWidth(3.0f);
  glColor3ub(30, 255, 90);
  glBegin(GL_LINE_STRIP);
  for (const cv::Vec3f& position : trajectory) {
    glVertex3f(position[0], position[1], position[2]);
  }
  glEnd();

  glPointSize(6.0f);
  glBegin(GL_POINTS);
  for (const cv::Vec3f& position : trajectory) {
    glVertex3f(position[0], position[1], position[2]);
  }
  glEnd();
  glPointSize(2.0f);
}

void DrawCameraFrustum(const Pose3f& pose, float scale,
                       unsigned char red, unsigned char green,
                       unsigned char blue) {
  const cv::Vec3f origin = pose.translation;
  const cv::Vec3f local_corners[4] = {
      cv::Vec3f(-0.7f * scale, -0.5f * scale, scale),
      cv::Vec3f( 0.7f * scale, -0.5f * scale, scale),
      cv::Vec3f( 0.7f * scale,  0.5f * scale, scale),
      cv::Vec3f(-0.7f * scale,  0.5f * scale, scale)};
  cv::Vec3f corners[4];
  for (int index = 0; index < 4; ++index) {
    corners[index] = TransformPoint(pose, local_corners[index]);
  }

  glColor3ub(red, green, blue);
  glLineWidth(1.5f);
  glBegin(GL_LINES);
  for (const cv::Vec3f& corner : corners) {
    glVertex3f(origin[0], origin[1], origin[2]);
    glVertex3f(corner[0], corner[1], corner[2]);
  }
  for (int index = 0; index < 4; ++index) {
    const cv::Vec3f& first = corners[index];
    const cv::Vec3f& second = corners[(index + 1) % 4];
    glVertex3f(first[0], first[1], first[2]);
    glVertex3f(second[0], second[1], second[2]);
  }
  glEnd();
}

void DrawKeyframeCameras(const std::vector<Pose3f>& keyframes,
                         const Pose3f& current_pose) {
  for (const Pose3f& keyframe : keyframes) {
    DrawCameraFrustum(keyframe, 0.055f, 20, 170, 60);
  }
  DrawCameraFrustum(current_pose, 0.09f, 40, 255, 100);
}

cv::Mat MakeEdgeOverlay(const cv::Mat& bgr_image,
                        const std::string& label) {
  cv::Mat overlay = bgr_image.clone();
  cv::Mat gray;
  cv::Mat edges;
  cv::cvtColor(bgr_image, gray, cv::COLOR_BGR2GRAY);
  cv::Canny(gray, edges, 60.0, 140.0);
  overlay.setTo(cv::Scalar(20, 235, 75), edges);
  cv::putText(overlay, label, cv::Point(18, 38),
              cv::FONT_HERSHEY_SIMPLEX, 0.85,
              cv::Scalar(20, 240, 80), 2, cv::LINE_AA);
  return overlay;
}

////////////////////////////////////////////////////////////////////////////////

#ifndef CALICAM_PDI_NO_MAIN

bool live = true; // To run live mode, you need a CaliCam from www.astar.ai

int main(int argc, char** argv) {
  try {
    std::string param_name = "../astar_calicam.yml";
    std::string image_name = "../wm_garden.jpg";
    int camera_index = 0;

    if (argc >= 2) {
      param_name = argv[1];
    }
    if (argc >= 3) {
      const std::string second_argument = argv[2];
      if (second_argument == "--image") {
        if (argc < 4) {
          throw std::runtime_error("--image requires an image filename");
        }
        live = false;
        image_name = argv[3];
      } else {
        camera_index = std::stoi(second_argument);
      }
    }

    cv::setUseOptimized(true);
    LoadParameters(param_name);
    InitRectifyMap();
    InitPointCloudLookup();

    PanoramaWindow scene(1100, 760, "A*SLAM-style CaliCam Map");
    std::cout << "3D controls: left drag rotates, middle drag pans, "
              << "right drag/scroll zooms, R resets, Q/Esc quits.\n"
              << "Mapping controls (Fisheye Image window): M toggles mapping, "
              << "C clears the map, S saves calicam_map.ply.\n";

    cv::VideoCapture vcapture;
    cv::Mat raw_img;
    if (live) {
#ifdef _WIN32
      vcapture.open(camera_index, cv::CAP_DSHOW);
#else
      vcapture.open(camera_index, cv::CAP_V4L2);
#endif

      if (!vcapture.isOpened()) {
        throw std::runtime_error(
            "Could not open camera index " + std::to_string(camera_index));
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
                << "Requested capture: " << cap_cols << " x " << cap_rows
                << '\n'
                << "Reported capture: "
                << vcapture.get(cv::CAP_PROP_FRAME_WIDTH) << " x "
                << vcapture.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;
    } else {
      raw_img = cv::imread(image_name, cv::IMREAD_COLOR);
      if (raw_img.empty()) {
        throw std::runtime_error("Could not open image: " + image_name);
      }
      if (half_size) {
        cv::resize(raw_img, raw_img, cv::Size(), 0.5, 0.5);
      }
    }

    const std::string win_name = "Fisheye Image";
    cv::namedWindow(win_name, cv::WINDOW_NORMAL);
    cv::createTrackbar("Num Disp: 16 + 16 *", win_name,
                       nullptr, ndisp_max, OnTrackNdisp);
    cv::setTrackbarPos("Num Disp: 16 + 16 *", win_name, ndisp_bar);
    cv::createTrackbar("Block Size: 5 + 2 *", win_name,
                       nullptr, wsize_max, OnTrackWsize);
    cv::setTrackbarPos("Block Size: 5 + 2 *", win_name, wsize_bar);
    cv::createTrackbar("Threshold: 70 +", win_name,
                       nullptr, thr_max, OnTrackThreshold);
    cv::setTrackbarPos("Threshold: 70 +", win_name, thr_bar);

    cv::Mat raw_imgl, raw_imgr, ll_imgl, ll_imgr, disp_img;
    std::vector<PCL> pcl_vec;
    TemporalMapper mapper;
    bool mapping_mode = false;

    while (scene.alive()) {
      if (live) {
        vcapture >> raw_img;
      }

      if (raw_img.empty()) {
        throw std::runtime_error("Image capture error");
      }
      if (raw_img.cols != cap_cols || raw_img.rows != cap_rows) {
        throw std::runtime_error(
            "Incorrect frame size: received " +
            std::to_string(raw_img.cols) + " x " +
            std::to_string(raw_img.rows) + ", expected " +
            std::to_string(cap_cols) + " x " +
            std::to_string(cap_rows) +
            ". Try another camera index or capture format.");
      }

      raw_img(cv::Rect(0, 0, img_width, cap_rows)).copyTo(raw_imgl);
      raw_img(cv::Rect(img_width, 0, img_width, cap_rows)).copyTo(raw_imgr);

      cv::remap(raw_imgl, ll_imgl, lmap[0][0], lmap[0][1],
                cv::INTER_LINEAR);
      cv::remap(raw_imgr, ll_imgr, lmap[1][0], lmap[1][1],
                cv::INTER_LINEAR);

      DisparityImage(ll_imgl, ll_imgr, disp_img);
      PointClouds(disp_img, ll_imgl, pcl_vec);

      if (mapping_mode) {
        mapper.ProcessFrame(ll_imgl, disp_img, pcl_vec);
      }

      if (scene.start_draw()) {
        if (mapping_mode) {
          DrawMonochromeMap(mapper.map_points());
          DrawTrajectory(mapper.trajectory());
          DrawKeyframeCameras(mapper.keyframe_poses(), mapper.world_pose());
        } else {
          DrawScene(pcl_vec);
        }
        cv::Mat inset_image =
            MakeEdgeOverlay(raw_imgl, "CALICAM FRONT");
        scene.draw_image_inset(inset_image, 0, 1);
        scene.finish_draw();
      }

      cv::Mat display_image = raw_imgl.clone();
      const cv::Scalar status_color =
          !mapping_mode ? cv::Scalar(230, 230, 230)
                        : (mapper.tracking_ok() ? cv::Scalar(80, 255, 80)
                                                : cv::Scalar(80, 80, 255));
      const std::string status_text =
          mapping_mode ? "MAP ON | " + mapper.status()
                       : "MAP OFF | press M to start";
      cv::putText(display_image, status_text, cv::Point(16, 30),
                  cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(0, 0, 0),
                  3, cv::LINE_AA);
      cv::putText(display_image, status_text, cv::Point(16, 30),
                  cv::FONT_HERSHEY_SIMPLEX, 0.65, status_color,
                  1, cv::LINE_AA);
      cv::imshow(win_name, display_image);
      const int key = cv::waitKeyEx(1);
      const int character = key & 0xff;
      if (character == 'q' || character == 'Q' || character == 27) {
        break;
      } else if (character == 'm' || character == 'M') {
        mapping_mode = !mapping_mode;
        if (mapping_mode) {
          mapper.Reset();
          std::cout << "Mapping started. Move the camera slowly.\n";
        } else {
          std::cout << "Mapping stopped.\n";
        }
      } else if ((character == 'c' || character == 'C') && mapping_mode) {
        mapper.Reset();
        std::cout << "Map cleared.\n";
      } else if (character == 's' || character == 'S') {
        if (mapper.SavePly("calicam_map.ply")) {
          std::cout << "Saved calicam_map.ply with "
                    << mapper.voxel_count() << " averaged voxels.\n";
        } else {
          std::cout << "Map is empty or calicam_map.ply could not be written.\n";
        }
      }
    }

    cv::destroyAllWindows();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << std::endl;
    return -1;
  }
}

#endif  // CALICAM_PDI_NO_MAIN

////////////////////////////////////////////////////////////////////////////////
