// DicomViewer.h
#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
// #include "glad/glad.h"
#include "tape_scanner/dicom_viewer/GLShader.h"
#include <tape_scanner/scanner/ScanGeometry.hpp>
#include <tape_scanner/scanner/WindowLevel.hpp>
#include <wx/glcanvas.h>
// #include <GL/gl.h>
#include <opencv2/opencv.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class GLImgViewer : public wxGLCanvas {
public:
    GLImgViewer(wxWindow *parent);
               
    void SetImage(const cv::Mat& img,  bool need_reset = false);
    void SetWindow(double center, double width);
    void SetInvert(bool invert);
    /// Меняет поворот и отражение и заново кладёт уже открытый кадр в текстуру.
    void SetOrientation(int rotation_deg, int flip);
    void RememberOrientation(int rotation_deg, int flip) { m_rotation = rotation_deg; m_flip = flip; }
    WindowLevel CurrentWindow() const { return m_window; }
    bool Inverted() const { return m_invert; }
    void SetWindowHandler(std::function<void(int center, int width)> handler) { m_on_window = std::move(handler); }
    /// Два левых щелчка: число пикселей сохранённой сетки по X (столбцы) или Y (строки).
    void BeginRuler(bool along_x);
    void CancelRuler();
    void SetRulerHandler(std::function<void(bool along_x, double pixels)> handler) { m_on_ruler = std::move(handler); }

    ~GLImgViewer();
private:
    class Tile {
    public:
        GLuint textureID;
        int x, y;
        int width, height;
        unsigned int m_VBO;
        unsigned int m_VAO;


        Tile(const cv::Mat& tileData, float x, float y, float w, float h);
        ~Tile();

        void Render(float scale, wxPoint2DDouble offset);
        void SetNearest(bool nearest);
    };

    class ImageTiler {
    public:
        ImageTiler(const cv::Mat& image);
        void Render(float scale, const wxPoint& offset, const wxSize& viewport);
        void SetNearest(bool nearest);

    private:
        std::vector<std::unique_ptr<Tile>> tiles;
        int tileSize;
        
        void CreateTiles(const cv::Mat& image);
        bool IsTileVisible(const std::unique_ptr<Tile>& tile, const wxRect& area);
        void RenderTile(const std::unique_ptr<Tile>& tile, float scale, wxPoint2DDouble offset);
    };
    
    wxGLContext* m_context;
    float m_scale = 1.0f;
    glm::mat4 m_projection;
    glm::mat4 m_view = 1.0f;
    glm::vec2 m_offset = {0.0f, 0.0f};
    bool m_dragging = false;
    bool m_window_dragging = false;
    std::unique_ptr<GLShader> m_shader;
    std::unique_ptr<ImageTiler> m_tiler;
    glm::vec2 m_lastMousePos;
    wxString m_cur_dcm_path;
    int m_cur_frame = 0;
    wxSize m_full_img_size;
    WindowLevel m_window;
    bool m_invert = false;
    int m_rotation = 0;
    int m_flip = 0;
    bool m_mono = true;
    cv::Mat m_source;
    std::function<void(int, int)> m_on_window;
    bool m_ruler_armed = false;
    bool m_ruler_along_x = true;
    bool m_ruler_has_first = false;
    double m_ruler_x = 0;
    double m_ruler_y = 0;
    std::function<void(bool, double)> m_on_ruler;

    void UploadPresented();
    void InitGL();
    void BindEvents();
    void UnBindEvents();

    void OnPaint(wxPaintEvent& event);
    void OnResize(wxSizeEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);

    void UpdateProjection();
    glm::vec2 ScreenToWorld(glm::vec2 screen_pos);
    void ScaleImage(bool is_expand, glm::vec2 curMousePos);
    bool RecordRulerPoint(glm::vec2 screen_pos);
};