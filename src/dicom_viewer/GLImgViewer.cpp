#include <iostream>
#include "tape_scanner/dicom_viewer/GLImgViewer.h"
#include <tape_scanner/scanner/DicomNumbers.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace
{
glm::mat4 getProjectionMatrix(int windowWidth, int windowHeight, int imageWidth, int imageHeight) {
    glm::mat4 projection;
    float imageAspect = imageWidth / imageHeight;
    float windowAspect = (float)windowWidth / (float)windowHeight;

    if (windowAspect > imageAspect) {
        // Окно шире, чем изображение - масштабируем по высоте
        float scale = windowAspect / imageAspect;
        projection = glm::ortho(-scale, scale, -1.0f, 1.0f, -1.0f, 1.0f);
    } else {
        // Окно уже, чем изображение - масштабируем по ширине
        float scale = imageAspect / windowAspect;
        projection = glm::ortho(-1.0f, 1.0f, -scale, scale, -1.0f, 1.0f);
    }

    return projection;
}
}

// Tile implementation
GLImgViewer::Tile::Tile(const cv::Mat& tileData, float x, float y, float w, float h) 
    : width(tileData.cols), height(tileData.rows)
{
    float vertices[] = {
        // X    Y      U    V
        x,      y,       0.0f, 0.0f,  // A (верх-лево)
        x + w,  y,       1.0f, 0.0f,  // B (верх-право)
        x,      y - h,   0.0f, 1.0f,  // C (низ-лево)
        x + w,  y - h,   1.0f, 1.0f   // D (низ-право)
    };

    get_logger() << "Tile(" << x << ", " << y << ", " << w << ", " << h << ") created\n";

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Атрибут 0: координаты вершин (X, Y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Атрибут 1: текстурные координаты (U, V)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLint internal_format, format, type;
    if (tileData.type() == CV_16UC1) {
        internal_format = GL_R16;
        format = GL_RED;
        type = GL_UNSIGNED_SHORT;
    } else if (tileData.type() == CV_8UC1) {
        internal_format = GL_R8;
        format = GL_RED;
        type = GL_UNSIGNED_BYTE;
    } else if (tileData.type() == CV_8UC3) {
        internal_format = GL_RGB;
        format = GL_RGB;
        type = GL_UNSIGNED_BYTE;
    } else {
        throw std::invalid_argument("Unsupported image format");
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, type == GL_UNSIGNED_SHORT ? 2 : 1);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, 
                width, height, 0,
                format, type, tileData.data);
}

GLImgViewer::Tile::~Tile() {
    glDeleteTextures(1, &textureID);
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
}

void GLImgViewer::Tile::Render(float scale, wxPoint2DDouble offset)
{
    // bind Texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    // render container
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // Рисуем квад    
}

void GLImgViewer::Tile::SetNearest(bool nearest)
{
    glBindTexture(GL_TEXTURE_2D, textureID);
    const GLint filter = nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

// ImageTiler implementation
GLImgViewer::ImageTiler::ImageTiler(const cv::Mat& image)
{
    GLint maxTexSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    cv::Mat upload = image;
    const DisplayTilePlan plan = PlanDisplayTiles(image.cols, image.rows, static_cast<int>(maxTexSize));
    if (plan.upload_cols > 0 && plan.upload_rows > 0 &&
        (plan.upload_cols != image.cols || plan.upload_rows != image.rows)) {
        cv::resize(image, upload, cv::Size(plan.upload_cols, plan.upload_rows), 0, 0, cv::INTER_AREA);
    }
    tileSize = std::max(plan.tile, 1);
    if (maxTexSize > 0) {
        tileSize = std::min(tileSize, static_cast<int>(maxTexSize));
    }
    CreateTiles(upload);
}

void GLImgViewer::ImageTiler::Render(float scale, const wxPoint& offset, const wxSize& viewport) {
    glEnable(GL_TEXTURE_2D);
    wxRect visibleArea(
        static_cast<int>((-offset.x) / scale),
        static_cast<int>((-offset.y) / scale),
        static_cast<int>(viewport.x / scale),
        static_cast<int>(viewport.y / scale)
    );

    wxPoint2DDouble gl_offset;
    gl_offset.m_x = offset.x / static_cast<double>(viewport.GetWidth());
    gl_offset.m_y = -(offset.y / static_cast<double>(viewport.GetHeight()));

    for(const auto& tile : tiles) {
        // if(IsTileVisible(tile, visibleArea)) 
        {
            RenderTile(tile, scale, gl_offset);
        }
    }
    glDisable(GL_TEXTURE_2D);
}

void GLImgViewer::ImageTiler::SetNearest(bool nearest) {
    for (const auto &tile : tiles) {
        tile->SetNearest(nearest);
    }
}

void GLImgViewer::ImageTiler::CreateTiles(const cv::Mat& image) {
    int cols = (image.cols + tileSize - 1) / tileSize;
    int rows = (image.rows + tileSize - 1) / tileSize;
    float x_pixel_size = 2.0f / image.cols;
    float y_pixel_size = 2.0f / image.rows;

    for(int y = 0; y < rows; y++) {
        for(int x = 0; x < cols; x++) {
            int tileX = x * tileSize;
            int tileY = y * tileSize;
            int w = std::min(tileSize, image.cols - tileX);
            int h = std::min(tileSize, image.rows - tileY);

            if(w <= 0 || h <= 0) continue;

            cv::Rect roi(tileX, tileY, w, h);
            cv::Mat tileData = image(roi).clone();
            float gl_x = tileX * x_pixel_size - 1.0;
            float gl_y = -tileY * y_pixel_size + 1.0;
            float gl_w = w * x_pixel_size;
            float gl_h = h * y_pixel_size;

            tiles.emplace_back(new Tile(tileData, gl_x, gl_y, gl_w, gl_h));
        }
    }
}

bool GLImgViewer::ImageTiler::IsTileVisible(const std::unique_ptr<Tile>& tile, const wxRect& area) {
    return !(tile->x + tile->width < area.GetLeft() - 500 ||
           tile->x > area.GetRight() + 500 ||
           tile->y + tile->height < area.GetTop() - 500 ||
           tile->y > area.GetBottom() + 500);
}

void GLImgViewer::ImageTiler::RenderTile(const std::unique_ptr<Tile>& tile, float scale, wxPoint2DDouble offset) {
    // float screenX = tile->x * scale + offset.x;
    // float screenY = tile->y * scale + offset.y;
    // float screenW = tile->width * scale;
    // float screenH = tile->height * scale;

    tile->Render(scale, offset);
}

// GLImageCanvas implementation
GLImgViewer::GLImgViewer(wxWindow *parent)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE),
        m_context(new wxGLContext(this))
{
    write_log("GLImgViewer::GLImgViewer start");
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
    InitGL();
    BindEvents();
    write_log("GLImgViewer::GLImgViewer finish");
}

GLImgViewer::~GLImgViewer() {
    delete m_context;
}

void GLImgViewer::SetWindow(double center, double width) {
    m_window.center = std::clamp(center, 0.0, kDisplayMaxValue);
    m_window.width = std::clamp(width, 1.0, kDisplayMaxValue);
    Refresh();
}

void GLImgViewer::SetInvert(bool invert) {
    m_invert = invert;
    Refresh();
}

void GLImgViewer::SetOrientation(int rotation_deg, int flip) {
    m_rotation = rotation_deg;
    m_flip = flip;
    if (!m_source.empty()) {
        UploadPresented();
    }
    Refresh();
}

void GLImgViewer::UploadPresented() {
    SetCurrent(*m_context);
    cv::Mat presented = PresentFrame(m_source, m_rotation, m_flip);
    m_mono = presented.channels() == 1;
    m_tiler = std::make_unique<ImageTiler>(presented);
    m_full_img_size = {presented.cols, presented.rows};
    if (m_tiler) {
        m_tiler->SetNearest(UseNearestFilter(m_scale));
    }
}

void GLImgViewer::BeginRuler(bool along_x) {
    m_ruler_armed = true;
    m_ruler_along_x = along_x;
    m_ruler_has_first = false;
}

void GLImgViewer::CancelRuler() {
    m_ruler_armed = false;
    m_ruler_has_first = false;
}

bool GLImgViewer::RecordRulerPoint(glm::vec2 screen_pos) {
    if (m_source.empty() || m_full_img_size.x <= 0 || m_full_img_size.y <= 0) {
        return false;
    }
    const glm::vec2 world = ScreenToWorld(screen_pos);
    double presented_x = 0;
    double presented_y = 0;
    if (!ImagePixelFromModel(world.x, world.y, m_full_img_size.x, m_full_img_size.y, presented_x, presented_y)) {
        return false;
    }
    double src_x = 0;
    double src_y = 0;
    if (!SourceMarkFromPresented(presented_x, presented_y, m_source.cols, m_source.rows, m_rotation, m_flip, src_x, src_y)) {
        return false;
    }
    if (!m_ruler_has_first) {
        m_ruler_x = src_x;
        m_ruler_y = src_y;
        m_ruler_has_first = true;
        return true;
    }
    const double span = AxisPixelSpan(m_ruler_x, m_ruler_y, src_x, src_y, m_ruler_along_x);
    m_ruler_armed = false;
    m_ruler_has_first = false;
    if (m_on_ruler) {
        m_on_ruler(m_ruler_along_x, span);
    }
    return true;
}

void GLImgViewer::SetImage(const cv::Mat& img, bool need_reset){
    m_ruler_has_first = false;
    UnBindEvents();
    m_source = img.clone();
    if (need_reset) {
        m_offset = {0, 0};
        m_scale = 1.0f;
        m_dragging = false;
        m_window_dragging = false;
    }
    UploadPresented();
    BindEvents();
    Refresh();
}

void GLImgViewer::InitGL() {
    write_log("InitGL start");
    SetCurrent(*m_context);

    // Инициализация GLAD
    if (!gladLoadGL()) {
        wxLogError("Failed to initialize GLAD");
    }

    m_shader = std::make_unique<GLShader>("vertex_shader.vs", "fragment_shader.fs");    

    glEnable(GL_TEXTURE_2D);
    write_log("after glEnable");
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    write_log("InitGL finish");
}

void GLImgViewer::BindEvents() {
    Bind(wxEVT_PAINT, &GLImgViewer::OnPaint, this);
    Bind(wxEVT_SIZE, &GLImgViewer::OnResize, this);
    Bind(wxEVT_MOUSEWHEEL, &GLImgViewer::OnMouseWheel, this);
    Bind(wxEVT_MOTION, &GLImgViewer::OnMouseMove, this);
    Bind(wxEVT_LEFT_DOWN, &GLImgViewer::OnMouseDown, this);
    Bind(wxEVT_LEFT_UP, &GLImgViewer::OnMouseUp, this);
    Bind(wxEVT_RIGHT_DOWN, &GLImgViewer::OnMouseDown, this);
    Bind(wxEVT_RIGHT_UP, &GLImgViewer::OnMouseUp, this);
}

void GLImgViewer::UnBindEvents() {
    Unbind(wxEVT_PAINT, &GLImgViewer::OnPaint, this);
    Unbind(wxEVT_SIZE, &GLImgViewer::OnResize, this);
    Unbind(wxEVT_MOUSEWHEEL, &GLImgViewer::OnMouseWheel, this);
    Unbind(wxEVT_MOTION, &GLImgViewer::OnMouseMove, this);
    Unbind(wxEVT_LEFT_DOWN, &GLImgViewer::OnMouseDown, this);
    Unbind(wxEVT_LEFT_UP, &GLImgViewer::OnMouseUp, this);
    Unbind(wxEVT_RIGHT_DOWN, &GLImgViewer::OnMouseDown, this);
    Unbind(wxEVT_RIGHT_UP, &GLImgViewer::OnMouseUp, this);
}

void GLImgViewer::OnPaint(wxPaintEvent& event) {
    wxPaintDC(this);
    SetCurrent(*m_context);
    
    glClearColor(0.02f, 0.02f, 0.02f, 1.0f);
    glViewport(0, 0, GetClientSize().x, GetClientSize().y);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, GetClientSize().x, GetClientSize().y, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();    
    
    if(m_tiler)
    {
        UpdateProjection();
        m_shader->use();
        const float max_value = m_mono && m_source.type() == CV_8UC1 ? 255.f : static_cast<float>(kDisplayMaxValue);
        m_shader->setFloat("uWindowCenter", static_cast<float>(m_window.center) / max_value);
        m_shader->setFloat("uWindowWidth", static_cast<float>(m_window.width) / max_value);
        m_shader->setInt("uInvert", m_invert ? 1 : 0);
        m_shader->setInt("uMono", m_mono ? 1 : 0);
        m_shader->setMatrix4f("uProjection", glm::value_ptr(m_projection));
        m_shader->setMatrix4f("uView", glm::value_ptr(m_view));
        m_shader->setMatrix4f("uModel", glm::value_ptr(glm::mat4(1.0f)));
        m_tiler->SetNearest(UseNearestFilter(m_scale));
        m_tiler->Render(m_scale, wxPoint(), GetClientSize());
    }
    SwapBuffers();
}

void GLImgViewer::OnResize(wxSizeEvent& event) {
    Refresh();
    event.Skip();
}

void GLImgViewer::OnMouseWheel(wxMouseEvent& event) {
    const float zoomFactor = 1.1f;
    const bool zoomIn = event.GetWheelRotation() > 0;
    ScaleImage(zoomIn, glm::vec2(event.GetPosition().x, event.GetPosition().y));

    Refresh();
}

void GLImgViewer::OnMouseDown(wxMouseEvent& event) {
    m_lastMousePos = glm::vec2(event.GetPosition().x, event.GetPosition().y);
    if (event.LeftDown() && m_ruler_armed && RecordRulerPoint(m_lastMousePos)) {
        return;
    }
    if (event.RightDown()) {
        m_window_dragging = true;
    } else {
        m_dragging = true;
    }
    if (!HasCapture()) {
        CaptureMouse();
    }
}

void GLImgViewer::OnMouseUp(wxMouseEvent& event) {
    m_dragging = false;
    m_window_dragging = false;
    if(HasCapture()) ReleaseMouse();
}

void GLImgViewer::OnMouseMove(wxMouseEvent& event) {
    glm::vec2 currentMousePos(event.GetPosition().x, event.GetPosition().y);
    if (m_window_dragging && event.RightIsDown()) {
        const double dx = currentMousePos.x - m_lastMousePos.x;
        const double dy = currentMousePos.y - m_lastMousePos.y;
        DragWindow(m_window, dx, dy, GetClientSize().GetWidth(), GetClientSize().GetHeight());
        m_lastMousePos = currentMousePos;
        if (m_on_window) {
            m_on_window(static_cast<int>(std::lround(m_window.center)), static_cast<int>(std::lround(m_window.width)));
        }
        Refresh();
        return;
    }
    if(m_dragging && event.Dragging() && event.LeftIsDown()) {
        glm::vec2 delta = ScreenToWorld(currentMousePos) - ScreenToWorld(m_lastMousePos);
        
        m_offset += delta;
        m_view = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale, m_scale, 1.0f));
        m_view = glm::translate(m_view, glm::vec3(m_offset, 0.0f));
        
        m_lastMousePos = currentMousePos;
        Refresh();
    }
}

// Обновление матрицы проекции с учетом пропорций
void GLImgViewer::UpdateProjection() {
    int width = GetClientSize().GetWidth();
    int height = GetClientSize().GetHeight();
    
    float windowAspect = (float)width / (float)height;
    float imageAspect = (float)m_full_img_size.x / (float)m_full_img_size.y;
    
    if (windowAspect > imageAspect) {
        // Окно шире, чем изображение - масштабируем по высоте
        float scale = windowAspect / imageAspect;
        m_projection = glm::ortho(-scale, scale, -1.0f, 1.0f, -1.0f, 1.0f);
    } else {
        // Окно уже, чем изображение - масштабируем по ширине
        float scale = imageAspect / windowAspect;
        m_projection = glm::ortho(-1.0f, 1.0f, -scale, scale, -1.0f, 1.0f);
    }
}

// Преобразование координат мыши в координаты сцены
glm::vec2 GLImgViewer::ScreenToWorld(glm::vec2 screen_pos) {
    int width = GetClientSize().GetWidth();
    int height = GetClientSize().GetHeight();    
    // Нормализованные координаты устройства
    glm::vec2 ndc(
        2.0f * screen_pos.x / width - 1.0f,
        1.0f - 2.0f * screen_pos.y / height
    );
    
    // Учитываем проекцию и view-матрицу
    glm::mat4 invVP = glm::inverse(m_projection * m_view);
    glm::vec4 worldPos = invVP * glm::vec4(ndc, 0.0f, 1.0f);
    
    return glm::vec2(worldPos);
}

void GLImgViewer::ScaleImage(bool is_expand, glm::vec2 cur_mouse_pos)
{
    glm::vec2 before_zoom_pos = ScreenToWorld(cur_mouse_pos);
    float zoomFactor = 1.1f;
    if(is_expand) {
        m_scale *= zoomFactor;
    } else {
        m_scale /= zoomFactor;
    }
    
    // Ограничиваем масштаб
    m_scale = glm::clamp(m_scale, 0.001f, 1000.0f);
    // Обновляем view-матрицу
    m_view = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale, m_scale, 1.0f));
    m_view = glm::translate(m_view, glm::vec3(m_offset, 0.0f));
    
    glm::vec2 after_zoom_pos = ScreenToWorld(cur_mouse_pos);
    glm::vec2 diff = after_zoom_pos - before_zoom_pos;
    
    m_offset+= diff;
    
    m_view = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale, m_scale, 1.0f));
    m_view = glm::translate(m_view, glm::vec3(m_offset, 0.0f));    
}
