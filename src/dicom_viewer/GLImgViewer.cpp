#include <iostream>
#include "tape_scanner/dicom_viewer/GLImgViewer.h"
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
    if (tileData.type() == CV_8UC1) {
        internal_format = GL_LUMINANCE;
        format = GL_LUMINANCE;
        type = GL_UNSIGNED_BYTE;
    } else if (tileData.type() == CV_8UC3) {
        internal_format = GL_RGB;
        format = GL_RGB;
        type = GL_UNSIGNED_BYTE;
    } else if (tileData.type() == CV_16UC1) {
        internal_format = GL_LUMINANCE16;
        format = GL_LUMINANCE;
        type = GL_UNSIGNED_SHORT;
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

// ImageTiler implementation
GLImgViewer::ImageTiler::ImageTiler(const cv::Mat& image)
{
    GLint maxTexSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    tileSize = std::min(maxTexSize, 4096);
    CreateTiles(image);
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

void GLImgViewer::SetImage(const cv::Mat& img, bool need_reset){
    UnBindEvents();

    m_tiler = std::make_unique<ImageTiler>(img);
    m_full_img_size = {img.cols, img.rows};
    
    if(need_reset)
    {
        m_offset = {0, 0};
        m_scale = 1.0f;
        m_dragging = false;
    }

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
}

void GLImgViewer::UnBindEvents() {
    Unbind(wxEVT_PAINT, &GLImgViewer::OnPaint, this);
    Unbind(wxEVT_SIZE, &GLImgViewer::OnResize, this);
    Unbind(wxEVT_MOUSEWHEEL, &GLImgViewer::OnMouseWheel, this);
    Unbind(wxEVT_MOTION, &GLImgViewer::OnMouseMove, this);
    Unbind(wxEVT_LEFT_DOWN, &GLImgViewer::OnMouseDown, this);
    Unbind(wxEVT_LEFT_UP, &GLImgViewer::OnMouseUp, this);
}

void GLImgViewer::OnPaint(wxPaintEvent& event) {
    wxPaintDC(this);
    SetCurrent(*m_context);
    
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
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
        m_shader->setFloat("uWindowCenter", m_bright);
        m_shader->setFloat("uWindowWidth", m_contrast);
        m_shader->setMatrix4f("uProjection", glm::value_ptr(m_projection));
        m_shader->setMatrix4f("uView", glm::value_ptr(m_view));
        m_shader->setMatrix4f("uModel", glm::value_ptr(glm::mat4(1.0f)));
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
    m_dragging = true;
    CaptureMouse();
}

void GLImgViewer::OnMouseUp(wxMouseEvent& event) {
    m_dragging = false;
    if(HasCapture()) ReleaseMouse();
}

void GLImgViewer::OnMouseMove(wxMouseEvent& event) {
    if(m_dragging && event.Dragging()) {
        glm::vec2 currentMousePos(event.GetPosition().x, event.GetPosition().y);
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
