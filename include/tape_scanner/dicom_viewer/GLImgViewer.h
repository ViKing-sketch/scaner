// DicomViewer.h
#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
// #include "glad/glad.h"
#include "tape_scanner/dicom_viewer/GLShader.h"
#include <wx/glcanvas.h>
// #include <GL/gl.h>
#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class GLImgViewer : public wxGLCanvas {
public:
    GLImgViewer(wxWindow *parent);
               
    void SetImage(const cv::Mat& img,  bool need_reset = false);
    void SetBright(float value = 1.0f){ m_bright = value; Refresh(); };
    void SetContrast(float value = 1.0f){ m_contrast = value; Refresh(); };

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
    };

    class ImageTiler {
    public:
        ImageTiler(const cv::Mat& image);
        void Render(float scale, const wxPoint& offset, const wxSize& viewport);

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
    std::unique_ptr<GLShader> m_shader;
    std::unique_ptr<ImageTiler> m_tiler;
    glm::vec2 m_lastMousePos;
    wxString m_cur_dcm_path;
    int m_cur_frame = 0;
    wxSize m_full_img_size;
    float m_bright = 1.0f;
    float m_contrast = 1.0f;

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
};