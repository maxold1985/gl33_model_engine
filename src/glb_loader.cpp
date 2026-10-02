#define WIN32_LEAN_AND_MEAN
#include "glb_loader.h"

#include <windows.h>
#include <objbase.h>
#include <wincodec.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/* ---------------------------------------------------------
   Minimal JSON parser.
   Enough for standard glTF 2.0 JSON contained in a .glb.
   --------------------------------------------------------- */

class Json {
public:
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Json> array;
    std::map<std::string, Json> object;

    const Json* get(const char* key) const
    {
        if (type != Type::Object)
            return nullptr;

        auto it = object.find(key);
        return it == object.end() ? nullptr : &it->second;
    }

    int as_int(int fallback = 0) const
    {
        return type == Type::Number ? (int)number : fallback;
    }

    std::size_t as_size(std::size_t fallback = 0) const
    {
        return type == Type::Number ? (std::size_t)number : fallback;
    }

    std::string as_string(const std::string& fallback = {}) const
    {
        return type == Type::String ? string : fallback;
    }
};

class JsonParser {
public:
    JsonParser(const char* begin, const char* end)
        : p(begin), e(end)
    {
    }

    Json parse()
    {
        skip_ws();
        Json result = parse_value();
        skip_ws();

        if (p != e)
            throw std::runtime_error("extra data after JSON");

        return result;
    }

private:
    const char* p;
    const char* e;

    void skip_ws()
    {
        while (p < e &&
               (*p == ' ' || *p == '\t' ||
                *p == '\r' || *p == '\n')) {
            ++p;
        }
    }

    bool consume(char c)
    {
        skip_ws();

        if (p < e && *p == c) {
            ++p;
            return true;
        }

        return false;
    }

    void expect(char c)
    {
        if (!consume(c))
            throw std::runtime_error("invalid JSON punctuation");
    }

    Json parse_value()
    {
        skip_ws();

        if (p >= e)
            throw std::runtime_error("unexpected end of JSON");

        if (*p == '{')
            return parse_object();

        if (*p == '[')
            return parse_array();

        if (*p == '"') {
            Json j;
            j.type = Json::Type::String;
            j.string = parse_string();
            return j;
        }

        if (*p == '-' || (*p >= '0' && *p <= '9'))
            return parse_number();

        if ((e - p) >= 4 && std::memcmp(p, "true", 4) == 0) {
            p += 4;
            Json j;
            j.type = Json::Type::Bool;
            j.boolean = true;
            return j;
        }

        if ((e - p) >= 5 && std::memcmp(p, "false", 5) == 0) {
            p += 5;
            Json j;
            j.type = Json::Type::Bool;
            j.boolean = false;
            return j;
        }

        if ((e - p) >= 4 && std::memcmp(p, "null", 4) == 0) {
            p += 4;
            return Json{};
        }

        throw std::runtime_error("invalid JSON value");
    }

    Json parse_object()
    {
        Json j;
        j.type = Json::Type::Object;

        expect('{');
        skip_ws();

        if (consume('}'))
            return j;

        for (;;) {
            skip_ws();

            if (p >= e || *p != '"')
                throw std::runtime_error("object key expected");

            std::string key = parse_string();

            expect(':');
            j.object.emplace(std::move(key), parse_value());

            if (consume('}'))
                break;

            expect(',');
        }

        return j;
    }

    Json parse_array()
    {
        Json j;
        j.type = Json::Type::Array;

        expect('[');
        skip_ws();

        if (consume(']'))
            return j;

        for (;;) {
            j.array.push_back(parse_value());

            if (consume(']'))
                break;

            expect(',');
        }

        return j;
    }

    std::string parse_string()
    {
        expect('"');

        std::string out;

        while (p < e) {
            unsigned char c = (unsigned char)*p++;

            if (c == '"')
                return out;

            if (c == '\\') {
                if (p >= e)
                    throw std::runtime_error("invalid JSON escape");

                const char esc = *p++;

                switch (esc) {
                    case '"':  out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/'; break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;

                    case 'u': {
                        if (e - p < 4)
                            throw std::runtime_error("invalid unicode escape");

                        unsigned value = 0;

                        for (int i = 0; i < 4; ++i) {
                            char h = *p++;
                            value <<= 4;

                            if (h >= '0' && h <= '9')
                                value |= (unsigned)(h - '0');
                            else if (h >= 'a' && h <= 'f')
                                value |= (unsigned)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F')
                                value |= (unsigned)(h - 'A' + 10);
                            else
                                throw std::runtime_error("invalid unicode hex");
                        }

                        if (value <= 0x7F) {
                            out += (char)value;
                        } else if (value <= 0x7FF) {
                            out += (char)(0xC0 | (value >> 6));
                            out += (char)(0x80 | (value & 0x3F));
                        } else {
                            out += (char)(0xE0 | (value >> 12));
                            out += (char)(0x80 | ((value >> 6) & 0x3F));
                            out += (char)(0x80 | (value & 0x3F));
                        }

                        break;
                    }

                    default:
                        throw std::runtime_error("unknown JSON escape");
                }
            } else {
                out += (char)c;
            }
        }

        throw std::runtime_error("unterminated JSON string");
    }

    Json parse_number()
    {
        skip_ws();

        const char* start = p;

        if (p < e && *p == '-')
            ++p;

        while (p < e && *p >= '0' && *p <= '9')
            ++p;

        if (p < e && *p == '.') {
            ++p;
            while (p < e && *p >= '0' && *p <= '9')
                ++p;
        }

        if (p < e && (*p == 'e' || *p == 'E')) {
            ++p;

            if (p < e && (*p == '+' || *p == '-'))
                ++p;

            while (p < e && *p >= '0' && *p <= '9')
                ++p;
        }

        std::string temp(start, p);

        Json j;
        j.type = Json::Type::Number;
        j.number = std::strtod(temp.c_str(), nullptr);
        return j;
    }
};

/* --------------------------------------------------------- */

static std::uint32_t read_u32_le(const std::uint8_t* p)
{
    return
        (std::uint32_t)p[0] |
        ((std::uint32_t)p[1] << 8) |
        ((std::uint32_t)p[2] << 16) |
        ((std::uint32_t)p[3] << 24);
}

static bool read_file(
    const std::string& path,
    std::vector<std::uint8_t>& out
)
{
    std::ifstream f(path, std::ios::binary);

    if (!f)
        return false;

    f.seekg(0, std::ios::end);
    const std::streamoff size = f.tellg();
    f.seekg(0, std::ios::beg);

    if (size <= 0)
        return false;

    out.resize((std::size_t)size);
    f.read((char*)out.data(), size);

    return !!f;
}

static std::string directory_of(const std::string& path)
{
    const std::size_t slash = path.find_last_of("\\/");

    if (slash == std::string::npos)
        return ".";

    return path.substr(0, slash);
}

static std::string join_path(
    const std::string& directory,
    const std::string& file
)
{
    if (file.size() >= 2 && file[1] == ':')
        return file;

    if (!file.empty() &&
        (file[0] == '\\' || file[0] == '/')) {
        return file;
    }

    return directory + "\\" + file;
}

static bool decode_wic_rgba(
    const std::uint8_t* bytes,
    std::size_t byte_count,
    GlbImage& out,
    std::string& error
)
{
    if (!bytes || byte_count == 0 ||
        byte_count > 0xFFFFFFFFu) {
        error = "invalid image data";
        return false;
    }

    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IWICImagingFactory,
        (void**)&factory
    );

    if (FAILED(hr)) {
        error = "WIC factory creation failed";
        return false;
    }

    hr = factory->CreateStream(&stream);

    if (SUCCEEDED(hr)) {
        hr = stream->InitializeFromMemory(
            (BYTE*)bytes,
            (DWORD)byte_count
        );
    }

    if (SUCCEEDED(hr)) {
        hr = factory->CreateDecoderFromStream(
            stream,
            nullptr,
            WICDecodeMetadataCacheOnLoad,
            &decoder
        );
    }

    if (SUCCEEDED(hr))
        hr = decoder->GetFrame(0, &frame);

    UINT width = 0;
    UINT height = 0;

    if (SUCCEEDED(hr))
        hr = frame->GetSize(&width, &height);

    if (SUCCEEDED(hr))
        hr = factory->CreateFormatConverter(&converter);

    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom
        );
    }

    if (SUCCEEDED(hr)) {
        out.width = (int)width;
        out.height = (int)height;
        out.rgba.resize((std::size_t)width * height * 4);

        hr = converter->CopyPixels(
            nullptr,
            width * 4,
            (UINT)out.rgba.size(),
            out.rgba.data()
        );
    }

    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();

    if (FAILED(hr)) {
        error = "WIC could not decode PNG/JPEG texture";
        out = GlbImage{};
        return false;
    }

    return true;
}

struct BufferView {
    std::size_t offset = 0;
    std::size_t length = 0;
    std::size_t stride = 0;
};

struct Accessor {
    int buffer_view = -1;
    std::size_t offset = 0;
    int component_type = 0;
    std::size_t count = 0;
    std::string type;
};

static const Json* array_item(
    const Json* array,
    int index
)
{
    if (!array ||
        array->type != Json::Type::Array ||
        index < 0 ||
        (std::size_t)index >= array->array.size()) {
        return nullptr;
    }

    return &array->array[(std::size_t)index];
}

static bool read_vec(
    const std::vector<std::uint8_t>& bin,
    const BufferView& view,
    const Accessor& accessor,
    std::size_t index,
    float* out,
    int components
)
{
    if (accessor.component_type != 5126)
        return false;

    const std::size_t packed =
        (std::size_t)components * sizeof(float);

    const std::size_t stride =
        view.stride ? view.stride : packed;

    const std::size_t start =
        view.offset +
        accessor.offset +
        index * stride;

    if (start + packed > bin.size())
        return false;

    std::memcpy(out, bin.data() + start, packed);
    return true;
}

static bool read_index(
    const std::vector<std::uint8_t>& bin,
    const BufferView& view,
    const Accessor& accessor,
    std::size_t index,
    std::uint32_t& out
)
{
    std::size_t component_size = 0;

    switch (accessor.component_type) {
        case 5121: component_size = 1; break;
        case 5123: component_size = 2; break;
        case 5125: component_size = 4; break;
        default: return false;
    }

    const std::size_t stride =
        view.stride ? view.stride : component_size;

    const std::size_t start =
        view.offset +
        accessor.offset +
        index * stride;

    if (start + component_size > bin.size())
        return false;

    if (component_size == 1) {
        out = bin[start];
    } else if (component_size == 2) {
        std::uint16_t v;
        std::memcpy(&v, bin.data() + start, 2);
        out = v;
    } else {
        std::uint32_t v;
        std::memcpy(&v, bin.data() + start, 4);
        out = v;
    }

    return true;
}


static int texture_source_index(
    const Json& root,
    int texture_index
)
{
    const Json* textures = root.get("textures");
    const Json* texture = array_item(textures, texture_index);

    if (!texture)
        return -1;

    const Json* source = texture->get("source");
    return source ? source->as_int(-1) : -1;
}

static bool load_image_index(
    const Json& root,
    int image_index,
    const std::vector<BufferView>& views,
    const std::vector<std::uint8_t>& bin,
    const std::string& glb_path,
    GlbImage& out_image,
    std::string& out_error
)
{
    const Json* images = root.get("images");
    const Json* image = array_item(images, image_index);

    if (!image) {
        out_error = "invalid image index";
        return false;
    }

    if (const Json* bv = image->get("bufferView")) {
        const int view_index = bv->as_int(-1);

        if (view_index < 0 ||
            (std::size_t)view_index >= views.size()) {
            out_error = "invalid image bufferView";
            return false;
        }

        const BufferView& view =
            views[(std::size_t)view_index];

        if (view.offset + view.length > bin.size()) {
            out_error = "embedded image outside BIN chunk";
            return false;
        }

        return decode_wic_rgba(
            bin.data() + view.offset,
            view.length,
            out_image,
            out_error
        );
    }

    if (const Json* uri = image->get("uri")) {
        const std::string uri_value =
            uri->as_string();

        if (uri_value.rfind("data:", 0) == 0) {
            out_error =
                "data URI images are not supported in this build";
            return false;
        }

        std::vector<std::uint8_t> bytes;

        if (!read_file(
                join_path(
                    directory_of(glb_path),
                    uri_value
                ),
                bytes)) {
            out_error =
                "could not open external image: " +
                uri_value;
            return false;
        }

        return decode_wic_rgba(
            bytes.data(),
            bytes.size(),
            out_image,
            out_error
        );
    }

    out_error = "image has no bufferView or URI";
    return false;
}

static bool read_primitive_geometry(
    const Json& primitive,
    const std::vector<Accessor>& accessors,
    const std::vector<BufferView>& views,
    const std::vector<std::uint8_t>& bin,
    GlbPrimitive& out,
    std::string& out_error
)
{
    const int mode =
        primitive.get("mode")
            ? primitive.get("mode")->as_int(4)
            : 4;

    if (mode != 4) {
        out_error =
            "only TRIANGLES primitives are supported";
        return false;
    }

    const Json* attributes =
        primitive.get("attributes");

    if (!attributes) {
        out_error = "primitive has no attributes";
        return false;
    }

    const Json* pos_j =
        attributes->get("POSITION");

    if (!pos_j) {
        out_error = "POSITION attribute is required";
        return false;
    }

    const int pos_index =
        pos_j->as_int(-1);

    const int normal_index =
        attributes->get("NORMAL")
            ? attributes->get("NORMAL")->as_int(-1)
            : -1;

    const int uv_index =
        attributes->get("TEXCOORD_0")
            ? attributes->get("TEXCOORD_0")->as_int(-1)
            : -1;

    const int indices_index =
        primitive.get("indices")
            ? primitive.get("indices")->as_int(-1)
            : -1;

    if (pos_index < 0 ||
        (std::size_t)pos_index >= accessors.size()) {
        out_error = "invalid POSITION accessor";
        return false;
    }

    const Accessor& pos =
        accessors[(std::size_t)pos_index];

    if (pos.buffer_view < 0 ||
        (std::size_t)pos.buffer_view >= views.size() ||
        pos.type != "VEC3" ||
        pos.component_type != 5126) {
        out_error =
            "POSITION must be FLOAT VEC3";
        return false;
    }

    out.vertices.resize(pos.count);

    for (std::size_t i = 0; i < pos.count; ++i) {
        float p3[3];

        if (!read_vec(
                bin,
                views[(std::size_t)pos.buffer_view],
                pos,
                i,
                p3,
                3)) {
            out_error = "could not read POSITION";
            return false;
        }

        GlbVertex& v = out.vertices[i];

        v.px = p3[0];
        v.py = p3[1];
        v.pz = p3[2];

        v.nx = 0.0f;
        v.ny = 0.0f;
        v.nz = 0.0f;

        v.u = 0.0f;
        v.v = 0.0f;
    }

    bool has_normals = false;

    if (normal_index >= 0 &&
        (std::size_t)normal_index < accessors.size()) {
        const Accessor& normal =
            accessors[(std::size_t)normal_index];

        if (normal.buffer_view >= 0 &&
            (std::size_t)normal.buffer_view < views.size() &&
            normal.type == "VEC3" &&
            normal.component_type == 5126 &&
            normal.count == pos.count) {

            has_normals = true;

            for (std::size_t i = 0; i < pos.count; ++i) {
                float n3[3];

                if (!read_vec(
                        bin,
                        views[(std::size_t)normal.buffer_view],
                        normal,
                        i,
                        n3,
                        3)) {
                    out_error = "could not read NORMAL";
                    return false;
                }

                out.vertices[i].nx = n3[0];
                out.vertices[i].ny = n3[1];
                out.vertices[i].nz = n3[2];
            }
        }
    }

    if (uv_index >= 0 &&
        (std::size_t)uv_index < accessors.size()) {
        const Accessor& uv =
            accessors[(std::size_t)uv_index];

        if (uv.buffer_view >= 0 &&
            (std::size_t)uv.buffer_view < views.size() &&
            uv.type == "VEC2" &&
            uv.component_type == 5126 &&
            uv.count == pos.count) {

            for (std::size_t i = 0; i < pos.count; ++i) {
                float t2[2];

                if (!read_vec(
                        bin,
                        views[(std::size_t)uv.buffer_view],
                        uv,
                        i,
                        t2,
                        2)) {
                    out_error = "could not read TEXCOORD_0";
                    return false;
                }

                out.vertices[i].u = t2[0];

                /*
                    WIC gives image rows top-to-bottom.
                    Flip V for conventional OpenGL upload.
                */
                out.vertices[i].v = 1.0f - t2[1];
            }
        }
    }

    if (indices_index >= 0 &&
        (std::size_t)indices_index < accessors.size()) {
        const Accessor& indices =
            accessors[(std::size_t)indices_index];

        if (indices.buffer_view < 0 ||
            (std::size_t)indices.buffer_view >= views.size()) {
            out_error = "invalid indices bufferView";
            return false;
        }

        out.indices.resize(indices.count);

        for (std::size_t i = 0; i < indices.count; ++i) {
            if (!read_index(
                    bin,
                    views[(std::size_t)indices.buffer_view],
                    indices,
                    i,
                    out.indices[i])) {
                out_error =
                    "indices must be U8/U16/U32";
                return false;
            }

            if (out.indices[i] >= out.vertices.size()) {
                out_error = "index outside vertex array";
                return false;
            }
        }
    } else {
        out.indices.resize(pos.count);

        for (std::size_t i = 0; i < pos.count; ++i)
            out.indices[i] = (std::uint32_t)i;
    }

    if (!has_normals) {
        for (std::size_t i = 0;
             i + 2 < out.indices.size();
             i += 3) {

            const std::uint32_t ia = out.indices[i + 0];
            const std::uint32_t ib = out.indices[i + 1];
            const std::uint32_t ic = out.indices[i + 2];

            GlbVertex& a = out.vertices[ia];
            GlbVertex& b = out.vertices[ib];
            GlbVertex& c = out.vertices[ic];

            const float abx = b.px - a.px;
            const float aby = b.py - a.py;
            const float abz = b.pz - a.pz;

            const float acx = c.px - a.px;
            const float acy = c.py - a.py;
            const float acz = c.pz - a.pz;

            const float nx =
                aby * acz - abz * acy;
            const float ny =
                abz * acx - abx * acz;
            const float nz =
                abx * acy - aby * acx;

            a.nx += nx; a.ny += ny; a.nz += nz;
            b.nx += nx; b.ny += ny; b.nz += nz;
            c.nx += nx; c.ny += ny; c.nz += nz;
        }

        for (GlbVertex& v : out.vertices) {
            const float length =
                std::sqrt(
                    v.nx * v.nx +
                    v.ny * v.ny +
                    v.nz * v.nz
                );

            if (length > 0.000001f) {
                v.nx /= length;
                v.ny /= length;
                v.nz /= length;
            } else {
                v.nx = 0.0f;
                v.ny = 1.0f;
                v.nz = 0.0f;
            }
        }
    }

    out.material_index =
        primitive.get("material")
            ? primitive.get("material")->as_int(-1)
            : -1;

    return true;
}

bool glb_load(
    const std::string& path,
    GlbModel& out_model,
    std::string& out_error
)
{
    out_model = GlbModel{};
    out_error.clear();

    std::vector<std::uint8_t> file;

    if (!read_file(path, file)) {
        out_error = "could not open GLB file";
        return false;
    }

    if (file.size() < 20) {
        out_error = "file too small for GLB 2.0";
        return false;
    }

    const std::uint32_t magic =
        read_u32_le(file.data() + 0);

    const std::uint32_t version =
        read_u32_le(file.data() + 4);

    const std::uint32_t declared_length =
        read_u32_le(file.data() + 8);

    if (magic != 0x46546C67u) {
        out_error = "invalid GLB magic";
        return false;
    }

    if (version != 2) {
        out_error = "only GLB 2.0 is supported";
        return false;
    }

    if (declared_length > file.size()) {
        out_error = "truncated GLB";
        return false;
    }

    const char* json_data = nullptr;
    std::size_t json_size = 0;
    std::vector<std::uint8_t> bin;

    std::size_t cursor = 12;

    while (cursor + 8 <= declared_length) {
        const std::uint32_t chunk_length =
            read_u32_le(file.data() + cursor + 0);

        const std::uint32_t chunk_type =
            read_u32_le(file.data() + cursor + 4);

        cursor += 8;

        if (cursor + chunk_length > declared_length) {
            out_error = "invalid GLB chunk size";
            return false;
        }

        if (chunk_type == 0x4E4F534Au) {
            json_data =
                (const char*)file.data() + cursor;

            json_size = chunk_length;
        } else if (chunk_type == 0x004E4942u) {
            bin.assign(
                file.begin() + (std::ptrdiff_t)cursor,
                file.begin() +
                    (std::ptrdiff_t)(cursor + chunk_length)
            );
        }

        cursor += chunk_length;
    }

    if (!json_data || json_size == 0) {
        out_error = "GLB has no JSON chunk";
        return false;
    }

    while (json_size > 0 &&
           (json_data[json_size - 1] == '\0' ||
            json_data[json_size - 1] == ' ')) {
        --json_size;
    }

    Json root;

    try {
        root = JsonParser(
            json_data,
            json_data + json_size
        ).parse();
    } catch (const std::exception& e) {
        out_error =
            std::string("JSON parse error: ") +
            e.what();

        return false;
    }

    const Json* buffer_views_json =
        root.get("bufferViews");

    const Json* accessors_json =
        root.get("accessors");

    const Json* meshes_json =
        root.get("meshes");

    if (!buffer_views_json ||
        !accessors_json ||
        !meshes_json) {
        out_error =
            "GLB missing bufferViews/accessors/meshes";
        return false;
    }

    std::vector<BufferView> views;

    for (const Json& j :
         buffer_views_json->array) {

        BufferView v;

        if (const Json* x = j.get("byteOffset"))
            v.offset = x->as_size();

        if (const Json* x = j.get("byteLength"))
            v.length = x->as_size();

        if (const Json* x = j.get("byteStride"))
            v.stride = x->as_size();

        views.push_back(v);
    }

    std::vector<Accessor> accessors;

    for (const Json& j :
         accessors_json->array) {

        Accessor a;

        if (const Json* x = j.get("bufferView"))
            a.buffer_view = x->as_int(-1);

        if (const Json* x = j.get("byteOffset"))
            a.offset = x->as_size();

        if (const Json* x = j.get("componentType"))
            a.component_type = x->as_int();

        if (const Json* x = j.get("count"))
            a.count = x->as_size();

        if (const Json* x = j.get("type"))
            a.type = x->as_string();

        accessors.push_back(a);
    }

    /*
       Materials are loaded independently.
       Every primitive keeps its own material_index.
    */
    const Json* materials_json =
        root.get("materials");

    if (materials_json &&
        materials_json->type == Json::Type::Array) {

        out_model.materials.resize(
            materials_json->array.size()
        );

        for (std::size_t i = 0;
             i < materials_json->array.size();
             ++i) {

            const Json& material =
                materials_json->array[i];

            GlbMaterial& dst =
                out_model.materials[i];

            const Json* pbr =
                material.get(
                    "pbrMetallicRoughness"
                );

            if (!pbr)
                continue;

            if (const Json* factor =
                    pbr->get("baseColorFactor")) {

                if (factor->type == Json::Type::Array &&
                    factor->array.size() >= 4) {

                    for (int c = 0; c < 4; ++c) {
                        dst.base_color_factor[c] =
                            (float)
                            factor->array[
                                (std::size_t)c
                            ].number;
                    }
                }
            }

            if (const Json* metallic =
                    pbr->get("metallicFactor")) {
                if (metallic->type ==
                    Json::Type::Number) {
                    dst.metallic_factor =
                        (float)metallic->number;
                }
            }

            if (const Json* roughness =
                    pbr->get("roughnessFactor")) {
                if (roughness->type ==
                    Json::Type::Number) {
                    dst.roughness_factor =
                        (float)roughness->number;
                }
            }

            if (const Json* texture_ref =
                    pbr->get("baseColorTexture")) {

                const int texture_index =
                    texture_ref->get("index")
                        ? texture_ref->get("index")
                              ->as_int(-1)
                        : -1;

                const int image_index =
                    texture_source_index(
                        root,
                        texture_index
                    );

                if (image_index >= 0) {
                    std::string image_error;

                    /*
                       A bad texture does not discard
                       the mesh. The baseColorFactor
                       still renders.
                    */
                    load_image_index(
                        root,
                        image_index,
                        views,
                        bin,
                        path,
                        dst.base_color_image,
                        image_error
                    );
                }
            }
        }
    }

    /*
       Load every primitive from every mesh.
       This fixes Blender exports that split a mesh
       by material slots.
    */
    for (const Json& mesh :
         meshes_json->array) {

        const Json* primitives =
            mesh.get("primitives");

        if (!primitives ||
            primitives->type != Json::Type::Array)
            continue;

        for (const Json& primitive :
             primitives->array) {

            GlbPrimitive dst;
            std::string primitive_error;

            if (!read_primitive_geometry(
                    primitive,
                    accessors,
                    views,
                    bin,
                    dst,
                    primitive_error)) {

                /*
                   Skip unsupported primitive modes,
                   but keep loading the rest.
                */
                continue;
            }

            out_model.primitives.push_back(
                std::move(dst)
            );
        }
    }

    if (out_model.primitives.empty()) {
        out_error =
            "GLB has no supported TRIANGLES primitives";
        return false;
    }

    out_model.source_path = path;

    return true;
}
