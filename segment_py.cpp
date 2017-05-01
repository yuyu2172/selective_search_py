#include <iostream>
#include <map>
#include <boost/python.hpp>
//#include <boost/numpy.hpp>
#include <boost/python/numpy.hpp>
#include "segment/segment-image.h"




static int operator<(const rgb& x, const rgb& y)
{
    return (x.r << 16 | x.g << 8 | x.b) < (y.r << 16 | y.g << 8 | y.b);
}

namespace boost {
    namespace python {
static void check_image_format(const numpy::ndarray& input_image)
{
    const int nd = input_image.get_nd();
    if(nd != 3)
        throw std::runtime_error("input_image must be 3-dimensional");

    const int depth = input_image.shape(2);

    if(depth != 3)
        throw std::runtime_error("input_image must have rgb channel");

    if(input_image.get_dtype() != numpy::dtype::get_builtin<unsigned char>())
        throw std::runtime_error("dtype of input_image must be uint8");

    if(!input_image.get_flags() & numpy::ndarray::C_CONTIGUOUS)
        throw std::runtime_error("input_image must be C-style contiguous");
}

tuple segment(const numpy::ndarray& input_image, float sigma, float c, int min_size)
{
    check_image_format(input_image);

    const int h = input_image.shape(0);
    const int w = input_image.shape(1);

    // Convert to internal format
    image<rgb> seg_input_img(w, h);
    rgb* p = reinterpret_cast<rgb*>(input_image.get_data());
    std::copy(p, p + w * h, seg_input_img.data);

    int num_css;
    image<rgb> *seg_result_img = segment_image(&seg_input_img, sigma, c, min_size, &num_css);

    // Convert from internal format
    numpy::ndarray result_image = numpy::empty(input_image.get_nd(), input_image.get_shape(), input_image.get_dtype());
    std::copy(seg_result_img->data, seg_result_img->data + w * h, reinterpret_cast<rgb*>(result_image.get_data()));

    delete seg_result_img;
    return make_tuple<numpy::ndarray, int>(result_image, num_css);
}

tuple segment_label(const numpy::ndarray& input_image, float sigma, float c, int min_size)
{
    check_image_format(input_image);

    const int h = input_image.shape(0);
    const int w = input_image.shape(1);

    // Convert to internal format
    image<rgb> seg_input_img(w, h);
    rgb* p = reinterpret_cast<rgb*>(input_image.get_data());
    std::copy(p, p + w * h, seg_input_img.data);

    // Execute segmentation
    int num_css;
    image<rgb> *seg_result_img = segment_image(&seg_input_img, sigma, c, min_size, &num_css);

    // Convert per-region-color to label
    numpy::ndarray result_label = numpy::empty(2, input_image.get_shape(), numpy::dtype::get_builtin<int>());
    rgb* in_p  = seg_result_img->data;
    int* out_p = reinterpret_cast<int*>(result_label.get_data());

    std::map<rgb, int> color_label_map;
    int current_label = 0;
    for(int i = 0; i < w * h; ++i, ++in_p, ++out_p)
    {
        auto label = color_label_map.find(*in_p);
        if(label != color_label_map.end())
            *out_p = label->second;
        else
            color_label_map[*in_p] = (*out_p = current_label++);
    }

    delete seg_result_img;
    return make_tuple<numpy::ndarray, int>(result_label, num_css);
}

BOOST_PYTHON_MODULE(segment)
{
    numpy::initialize();
    def("segment", segment);
    def("segment_label", segment_label);
}

}
}
