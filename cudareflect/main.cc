/**
* @file main.cpp
* @author Supakorn "Jamie" Rassameemasmuang <jamievlin@outlook.com>
* Program for loading image and writing the irradiated image.
*/

#include "common.h"

#ifdef _WIN32
// use vcpkg getopt package for this
#undef _UNICODE
#include <getopt.h>
#else
#include <unistd.h>
#endif

#include "kernel.h"
#include "ReflectanceMapper.cuh"
#include "EXRFiles.h"

std::string const ARG_HELP = "./reflectance [mode] -f in_file -p out_file_prefix";

struct Args
{
    char mode = 0;
    char const* file_in = nullptr;
    char const* file_out_prefix = nullptr;

    bool validate()
    {
        if (mode == 0)
            return false;

        if ((mode == 'a' || mode == 'i' || mode == 'r') && !file_in)
        {
            return false;
        }
        return file_out_prefix != nullptr;
    }
};

Args parseArgs(int argc, char* argv[])
{
    Args arg;
    int c;
    while ((c = getopt(argc, argv, "airof:p:")) != -1)
    {
        switch (c)
        {
        case 'a':
            arg.mode = 'a';
            break;
        case 'i':
            arg.mode = 'i';
            break;
        case 'r':
            arg.mode = 'r';
            break;
        case 'o':
            arg.mode = 'o';
            break;
        case 'f':
            arg.file_in = optarg;
            break;
        case 'p':
            arg.file_out_prefix = optarg;
            break;
        case '?':
            std::cerr << ARG_HELP << std::endl;
            exit(0);
            break;
        default:
            std::cerr << ARG_HELP << std::endl;
            exit(1);
        }
    }

    if (!arg.validate())
    {
        std::cerr << ARG_HELP << std::endl;
        exit(1);
    }
    return arg;
}

struct image_t
{
    float4* im;
    int width, height;

    int sz() const { return width * height; }

    image_t(float4* im, int width, int height) :
        im(im), width(std::move(width)), height(std::move(height)) {}
};

void irradiate_im(image_t& im, std::string const& prefix)
{
    std::vector<float3> out_proc(im.sz());
    std::stringstream out_name;
    out_name << prefix;
    std::cout << "Irradiating image..." << std::endl;
    irradiate_ker(im.im, out_proc.data(), im.width, im.height);
    out_name << "_diffuse.exr";

    std::string out_name_str(std::move(out_name).str());
    OEXRFile ox(out_proc, im.width, im.height);

    std::cout << "copying data back" << std::endl;
    std::cout << "writing to: " << out_name_str << std::endl;
    ox.write(out_name_str);
}

void map_refl_im(image_t& im, std::string const& prefix, float const& step, int i)
{
    float roughness = step * i;
    std::vector<float3> out_proc(im.sz());
    std::stringstream out_name;
    out_name << prefix;
    std::cout << "Mapping reflectance map..." << std::endl;
    out_name << "_refl_" << std::fixed << std::setprecision(3) << step << "_" << i << ".exr";
    std::string out_name_str(std::move(out_name).str());


    map_reflectance_ker(im.im, out_proc.data(), im.width, im.height, roughness);
    OEXRFile ox(out_proc, im.width, im.height);
    std::cout << "copying data back" << std::endl;
    std::cout << "writing to: " << out_name_str << std::endl;
    ox.write(out_name_str);
}

int main(int argc, char* argv[])
{
    Args args = parseArgs(argc, argv);
    std::vector<float4> im_proc;
    size_t width = 0;
    size_t height = 0;

    if (args.file_in)
    {
        std::cout << "Loaded file " << args.file_in << std::endl;
        EXRFile im(args.file_in);
        width = im.getWidth();
        height = im.getHeight();

        for (size_t i = 0; i < height; ++i)
        {
            for (size_t j = 0; j < width; ++j)
            {
                // index is i*height+j <-> (i,j)
                im_proc.emplace_back(im.getPixel4(j, i));
            }
            // std::cout << "pushed row " << i << " into array" << std::endl;
        }
        std::cout << "finished converting to float3" << std::endl;
    }
    image_t imt(im_proc.data(), width, height);

    if (args.mode == 'o')
    {
        int res = 200;
        std::vector<float2> out_proc(res * res);

        std::cout << "generating Fresnel/Roughness/cos_v data" << std::endl;
        std::cout << "writing to " << args.file_out_prefix << ".exr" << std::endl;
        generate_brdf_integrate_lut_ker(res, res, out_proc.data());
        OEXRFile ox(out_proc, res, res);
        ox.write(std::string(args.file_out_prefix) + ".exr");
    }
    else
    {
        if (args.mode == 'r')
        {
            size_t count = 10;
            float step = 1.0f / count;
            for (int i = 1; i <= count; ++i)
            {
                map_refl_im(imt, args.file_out_prefix, step, i);
            }
        }
        else if (args.mode == 'i')
        {
            irradiate_im(imt, args.file_out_prefix);
        }
        else if (args.mode == 'a')
        {
            size_t count = 10;
            float step = 1.0f / count;
            for (int i = 1; i <= count; ++i)
            {
                map_refl_im(imt, args.file_out_prefix, step, i);
            }
            irradiate_im(imt, args.file_out_prefix);
        }
    }
}
