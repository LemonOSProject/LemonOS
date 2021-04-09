#include <Lemon/GUI/Widgets.h>

#include <Lemon/Graphics/Graphics.h>

namespace Lemon::GUI{
    //////////////////////////
    // Image
    //////////////////////////
    Image::Image(rect_t _bounds) : Widget(_bounds), texture(fixedBounds.size) {

    }

    void Image::Load(surface_t* image){
        texture.LoadSourcePixels(image);
    }

    int Image::Load(const char* path){
        surface_t surface;
        
        int err = Graphics::LoadImage(path, &surface);
        if(err){
            return err; // Error loading image from path
        }

        texture.AdoptSourcePixels(&surface); // We don't need the buffer so give it to the Texture object

        return 0;
    }

    void Image::Paint(surface_t* surface){
        texture.Blit(fixedBounds.pos, surface);
    }

    void Image::UpdateFixedBounds(){
        Widget::UpdateFixedBounds();

        texture.SetSize(fixedBounds.size);
    }
}