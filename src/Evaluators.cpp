// https://github.com/CedricGuillemet/Imogen
//
// The MIT License(MIT)
// 
// Copyright(c) 2018 Cedric Guillemet
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include <GL/gl3w.h>    // Initialize with gl3wInit()
#include "Evaluators.h"
#include "EvaluationStages.h"
#include "nfd.h"
#include "Bitmap.h"

Evaluators gEvaluators;

struct EValuationFunction
{
    const char *szFunctionName;
    void *function;
};

static const EValuationFunction evaluationFunctions[] = {
    { "Log", (void*)Log },
    { "ReadImage", (void*)Image::Read },
    { "WriteImage", (void*)Image::Write },
    { "GetEvaluationImage", (void*)EvaluationStages::GetEvaluationImage },
    { "SetEvaluationImage", (void*)EvaluationStages::SetEvaluationImage },
    { "SetEvaluationImageCube", (void*)EvaluationStages::SetEvaluationImageCube },
    { "AllocateImage", (void*)EvaluationStages::AllocateImage },
    { "FreeImage", (void*)Image::Free },
    { "SetThumbnailImage", (void*)EvaluationStages::SetThumbnailImage },
    //{ "Evaluate", (void*)EvaluationStages::Evaluate},
    { "SetBlendingMode", (void*)EvaluationStages::SetBlendingMode},
    { "EnableDepthBuffer", (void*)EvaluationStages::EnableDepthBuffer},
    { "GetEvaluationSize", (void*)EvaluationStages::GetEvaluationSize},
    { "SetEvaluationSize", (void*)EvaluationStages::SetEvaluationSize },
    { "SetEvaluationCubeSize", (void*)EvaluationStages::SetEvaluationCubeSize },
    { "AllocateComputeBuffer", (void*)EvaluationStages::AllocateComputeBuffer },
    { "CubemapFilter", (void*)Image::CubemapFilter},
    { "SetProcessing", (void*)EvaluationStages::SetProcessing},
    { "Job", (void*)EvaluationStages::Job },
    { "JobMain", (void*)EvaluationStages::JobMain },
    { "memmove", memmove },
    { "strcpy", strcpy },
    { "strlen", strlen },
    { "fabsf", fabsf },
    { "LoadSVG", (void*)Image::LoadSVG},
    { "LoadScene", (void*)EvaluationStages::LoadScene},
    { "SetEvaluationScene", (void*)EvaluationStages::SetEvaluationScene},
    { "GetEvaluationScene", (void*)EvaluationStages::GetEvaluationScene},
    { "GetEvaluationRenderer", (void*)EvaluationStages::GetEvaluationRenderer},
    { "InitRenderer", (void*)EvaluationStages::InitRenderer},
    { "UpdateRenderer", (void*)EvaluationStages::UpdateRenderer},
};

static void libtccErrorFunc(void *opaque, const char *msg)
{
    Log(msg);
    Log("\n");
}

void LogPython(const std::string &str)
{
    Log(str.c_str());
}

PYBIND11_MAKE_OPAQUE(Image);

struct PyGraph {
    Material* mGraph;
};

struct PyNode {
    Material* mGraph;
    MaterialNode* mNode;
    int mNodeIndex;
};

PYBIND11_EMBEDDED_MODULE(Imogen, m) 
{
    pybind11::class_<Image>(m, "Image");
    auto graph = pybind11::class_<PyGraph>(m, "Graph");
    graph.def("GetEvaluationList", [](PyGraph& pyGraph) {
        auto d = pybind11::list();

        for (int index = 0; index < int(pyGraph.mGraph->mMaterialNodes.size()) ; index++)
        {
            auto & node = pyGraph.mGraph->mMaterialNodes[index];
            d.append(new PyNode{ pyGraph.mGraph, &node, index});
        }
        return d;

    });
    auto node = pybind11::class_<PyNode>(m, "Node");
    node.def("GetType", [](PyNode& node) {
        std::string& s = node.mNode->mTypeName;
        if (!s.length())
            s = std::string("EmptyNode");
        return s;
    });
    node.def("GetInputs", [](PyNode& node) {
        //
        auto d = pybind11::list();
        if (node.mNode->mType == 0xFFFFFFFF)
            return d;

        MetaNode& metaNode = gMetaNodes[node.mNode->mType];

        for (auto& con : node.mGraph->mMaterialConnections)
        {
            if (con.mOutputNode == node.mNodeIndex)
            {
                auto e = pybind11::dict();
                d.append(e);

                e["nodeIndex"] = pybind11::int_(con.mInputNode);
                e["name"] = metaNode.mInputs[con.mOutputSlot].mName;
            }
        }
        return d;
    });
    node.def("GetParameters", [](PyNode& node) {
        // name, type, value
        auto d = pybind11::list();
        if (node.mNode->mType == 0xFFFFFFFF)
            return d;
        MetaNode& metaNode = gMetaNodes[node.mNode->mType];
        
        for (uint32_t index = 0;index<metaNode.mParams.size();index++)
        {
            auto& param = metaNode.mParams[index];
            auto e = pybind11::dict();
            d.append(e);
            e["name"] = param.mName;
            e["type"] = pybind11::int_(int(param.mType));

            size_t parameterOffset = GetParameterOffset(node.mNode->mType, index);
            if (parameterOffset >= node.mNode->mParameters.size())
            {
                e["value"] = std::string("");
                continue;
            }
            
            unsigned char * ptr = &node.mNode->mParameters[parameterOffset];
            float *ptrf = (float*)ptr;
            int * ptri = (int*)ptr;
            char tmps[512];
            switch (param.mType)
            {
            case Con_Float:
                e["value"] = pybind11::float_(ptrf[0]);
                break;
            case Con_Float2:
                sprintf(tmps, "%f,%f", ptrf[0], ptrf[1]);
                e["value"] = std::string(tmps);
                break;
            case Con_Float3:
                sprintf(tmps, "%f,%f,%f", ptrf[0], ptrf[1], ptrf[2]);
                e["value"] = std::string(tmps);
                break;
            case Con_Float4:
                sprintf(tmps, "%f,%f,%f,%f", ptrf[0], ptrf[1], ptrf[2], ptrf[3]);
                e["value"] = std::string(tmps);
                break;
            case Con_Color4:
                sprintf(tmps, "%f,%f,%f,%f", ptrf[0], ptrf[1], ptrf[2], ptrf[3]);
                e["value"] = std::string(tmps);
                break;
            case Con_Int:
                e["value"] = pybind11::int_(ptri[0]);
                break;
            case Con_Int2:
                sprintf(tmps, "%d,%d", ptri[0], ptri[1]);
                e["value"] = std::string(tmps);
                break;
            case Con_Ramp:
                e["value"] = std::string("N/A");
                break;
            case Con_Angle:
                e["value"] = pybind11::float_(ptrf[0]);
                break;
            case Con_Angle2:
                sprintf(tmps, "%f,%f", ptrf[0], ptrf[1]);
                e["value"] = std::string(tmps);
                break;
            case Con_Angle3:
                sprintf(tmps, "%f,%f,%f", ptrf[0], ptrf[1], ptrf[2]);
                e["value"] = std::string(tmps);
                break;
            case Con_Angle4:
                sprintf(tmps, "%f,%f,%f,%f", ptrf[0], ptrf[1], ptrf[2], ptrf[3]);
                e["value"] = std::string(tmps);
                break;
            case Con_Enum:
                e["value"] = pybind11::int_(ptri[0]);
                break;
            case Con_Structure:
                e["value"] = std::string("N/A");
                break;
            case Con_FilenameRead:
            case Con_FilenameWrite:
                e["value"] = std::string((char*)ptr, strlen((char*)ptr));
                break;
            case Con_ForceEvaluate:
                e["value"] = std::string("N/A");
                break;
            case Con_Bool:
                e["value"] = pybind11::bool_(ptr[0] != 0);
                break;
            case Con_Ramp4:
            case Con_Camera:
                e["value"] = std::string("N/A");
                break;
            }
        }
        return d;
    });
    m.def("RegisterPlugin", [](std::string& name, std::string command) {
        imogen.mRegisteredPlugins.push_back({ name, command });
        Log("Plugin registered : %s \n", name.c_str());
    });
    m.def("FileDialogRead", []() {
        nfdchar_t *outPath = NULL;
        nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);

        if (result == NFD_OKAY)
        {
            std::string res = outPath;
            free(outPath);
            return res;
        }
        return std::string();

    });
    m.def("FileDialogWrite", []() {
        nfdchar_t *outPath = NULL;
        nfdresult_t result = NFD_SaveDialog(NULL, NULL, &outPath);

        if (result == NFD_OKAY)
        {
            std::string res = outPath;
            free(outPath);
            return res;
        }
        return std::string();
    });
    m.def("Log", LogPython );
    m.def("ReadImage", Image::Read );
    m.def("WriteImage", Image::Write );
    m.def("GetEvaluationImage", EvaluationStages::GetEvaluationImage );
    m.def("SetEvaluationImage", EvaluationStages::SetEvaluationImage );
    m.def("SetEvaluationImageCube", EvaluationStages::SetEvaluationImageCube );
    m.def("AllocateImage", EvaluationStages::AllocateImage );
    m.def("FreeImage", Image::Free );
    m.def("SetThumbnailImage", EvaluationStages::SetThumbnailImage );
    //m.def("Evaluate", EvaluationStages::Evaluate );
    m.def("SetBlendingMode", EvaluationStages::SetBlendingMode );
    m.def("GetEvaluationSize", EvaluationStages::GetEvaluationSize );
    m.def("SetEvaluationSize", EvaluationStages::SetEvaluationSize );
    m.def("SetEvaluationCubeSize", EvaluationStages::SetEvaluationCubeSize );
    m.def("CubemapFilter", Image::CubemapFilter );
    m.def("SetProcessing", EvaluationStages::SetProcessing );
    /*
    m.def("Job", EvaluationStages::Job );
    m.def("JobMain", EvaluationStages::JobMain );
    */
    m.def("GetLibraryGraphs", []() {
        auto d = pybind11::list();
        for (auto& graph : library.mMaterials)
        {
            const std::string& s = graph.mName;
            d.append(graph.mName);
        }
        return d;
        }
        );
    m.def("GetGraph", [](const std::string& graphName) -> PyGraph* {
        
        for (auto& graph : library.mMaterials)
        {
            if (graph.mName == graphName)
            {
                return new PyGraph{ &graph };
            }
        }
        return nullptr;
    }
    );
    /*
    m.def("accessor_api", []() {
        auto d = pybind11::dict();

        d["target"] = 10;

        auto l = pybind11::list();
        l.append(5);
        l.append(-1);
        l.append(-1);
        d["inputs"] = l;

        return d;
    });
    */
    
    /*
    m.def("GetImage", []() {
        auto i = new Image;
        //pImage i;
        //i.a = 14;
        //printf("new img %p \n", &i);
        return i;
    });

    m.def("SaveImage", [](Image image) {
        //printf("Saving image %d\n", image.a);
        //printf("save img %p \n", image);
    });
    */
}
std::string Evaluators::GetEvaluator(const std::string& filename)
{
    return mEvaluatorScripts[filename].mText;
}

void Evaluators::SetEvaluators(const std::vector<EvaluatorFile>& evaluatorfilenames)
{
    ClearEvaluators();

    mEvaluatorPerNodeType.clear();
    mEvaluatorPerNodeType.resize(evaluatorfilenames.size(), Evaluator());

    // GLSL
    for (auto& file : evaluatorfilenames)
    {
        if (file.mEvaluatorType != EVALUATOR_GLSL&& file.mEvaluatorType != EVALUATOR_GLSLCOMPUTE)
            continue;
        const std::string filename = file.mFilename;

        std::ifstream t(file.mDirectory + filename);
        if (t.good())
        {
            std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
            if (mEvaluatorScripts.find(filename) == mEvaluatorScripts.end())
                mEvaluatorScripts[filename] = EvaluatorScript(str);
            else
                mEvaluatorScripts[filename].mText = str;
        }
    }

    // GLSL
    std::string baseShader = mEvaluatorScripts["Shader.glsl"].mText;
    for (auto& file : evaluatorfilenames)
    {
        if (file.mEvaluatorType != EVALUATOR_GLSL)
            continue;
        const std::string filename = file.mFilename;

        if (filename == "Shader.glsl")
            continue;

        EvaluatorScript& shader = mEvaluatorScripts[filename];
        std::string shaderText = ReplaceAll(baseShader, "__NODE__", shader.mText);
        std::string nodeName = ReplaceAll(filename, ".glsl", "");
        shaderText = ReplaceAll(shaderText, "__FUNCTION__", nodeName + "()");

        unsigned int program = LoadShader(shaderText, filename.c_str());

        int parameterBlockIndex = glGetUniformBlockIndex(program, (nodeName + "Block").c_str());
        if (parameterBlockIndex != -1)
            glUniformBlockBinding(program, parameterBlockIndex, 1);

        parameterBlockIndex = glGetUniformBlockIndex(program, "EvaluationBlock");
        if (parameterBlockIndex != -1)
            glUniformBlockBinding(program, parameterBlockIndex, 2);
        shader.mProgram = program;
        if (shader.mType != -1)
            mEvaluatorPerNodeType[shader.mType].mGLSLProgram = program;
    }

    if (!gEvaluationStateGLSLBuffer)
    {
        glGenBuffers(1, &gEvaluationStateGLSLBuffer);
        glBindBuffer(GL_UNIFORM_BUFFER, gEvaluationStateGLSLBuffer);

        glBufferData(GL_UNIFORM_BUFFER, sizeof(EvaluationInfo), NULL, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 2, gEvaluationStateGLSLBuffer);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    TagTime("GLSL init");

    // GLSL compute
    for (auto& file : evaluatorfilenames)
    {
        if (file.mEvaluatorType != EVALUATOR_GLSLCOMPUTE)
            continue;
        const std::string filename = file.mFilename;

        EvaluatorScript& shader = mEvaluatorScripts[filename];
        //std::string shaderText = ReplaceAll(baseShader, "__NODE__", shader.mText);
        std::string nodeName = ReplaceAll(filename, ".glslc", "");
        //shaderText = ReplaceAll(shaderText, "__FUNCTION__", nodeName + "()");

        unsigned int program = 0;
        if (nodeName == filename)
        {
            // glsl in compute directory
            nodeName = ReplaceAll(filename, ".glsl", "");
            program = LoadShader(shader.mText, filename.c_str());
        }
        else
        {
            program = LoadShaderTransformFeedback(shader.mText, filename.c_str());
        }

        int parameterBlockIndex = glGetUniformBlockIndex(program, (nodeName + "Block").c_str());
        if (parameterBlockIndex != -1)
            glUniformBlockBinding(program, parameterBlockIndex, 1);

        parameterBlockIndex = glGetUniformBlockIndex(program, "EvaluationBlock");
        if (parameterBlockIndex != -1)
            glUniformBlockBinding(program, parameterBlockIndex, 2);
        shader.mProgram = program;
        if (shader.mType != -1)
            mEvaluatorPerNodeType[shader.mType].mGLSLProgram = program;
    }
    TagTime("GLSL compute init");
    // C
    for (auto& file : evaluatorfilenames)
    {
        if (file.mEvaluatorType != EVALUATOR_C)
            continue;
        const std::string filename = file.mFilename;
        try
        {
            std::ifstream t(file.mDirectory + filename);
            if (!t.good())
            {
                Log("%s - Unable to load file.\n", filename.c_str());
                continue;
            }
            std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
            if (mEvaluatorScripts.find(filename) == mEvaluatorScripts.end())
                mEvaluatorScripts[filename] = EvaluatorScript(str);
            else
                mEvaluatorScripts[filename].mText = str;

            EvaluatorScript& program = mEvaluatorScripts[filename];
            TCCState *s = tcc_new();

            int *noLib = (int*)s;
            noLib[2] = 1; // no stdlib

            tcc_set_error_func(s, 0, libtccErrorFunc);
            tcc_add_include_path(s, "Nodes/C/");
            tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

            if (tcc_compile_string(s, program.mText.c_str()) != 0)
            {
                Log("%s - Compilation error!\n", filename.c_str());
                continue;
            }

            for (auto& evaluationFunction : evaluationFunctions)
                tcc_add_symbol(s, evaluationFunction.szFunctionName, evaluationFunction.function);

            int size = tcc_relocate(s, NULL);
            if (size == -1)
            {
                Log("%s - Libtcc unable to relocate program!\n", filename.c_str());
                continue;
            }
            program.mMem = malloc(size);
            tcc_relocate(s, program.mMem);

            *(void**)(&program.mCFunction) = tcc_get_symbol(s, "main");
            if (!program.mCFunction)
            {
                Log("%s - No main function!\n", filename.c_str());
            }
            tcc_delete(s);

            if (program.mType != -1)
            {
                mEvaluatorPerNodeType[program.mType].mCFunction = program.mCFunction;
                mEvaluatorPerNodeType[program.mType].mMem = program.mMem;
            }
        }
        catch (...)
        {
            Log("Error at compiling %s", filename.c_str());
        }
    }
    TagTime("C init");
    
    for (auto& file : evaluatorfilenames)
    {
        if (file.mEvaluatorType != EVALUATOR_PYTHON)
            continue;
        const std::string filename = file.mFilename;
        std::string nodeName = ReplaceAll(filename, ".py", "");
        EvaluatorScript& shader = mEvaluatorScripts[filename];
        try
        {
            shader.mPyModule = pybind11::module::import("Nodes.Python.testnode");
            if (shader.mType != -1)
                mEvaluatorPerNodeType[shader.mType].mPyModule = shader.mPyModule;
        }
        catch (...)
        {
            Log("Python exception\n");
        }
    }
    
    TagTime("Python init");
}

void Evaluators::ClearEvaluators()
{
    // clear
    for (auto& program : mEvaluatorPerNodeType)
    {
        if (program.mGLSLProgram)
            glDeleteProgram(program.mGLSLProgram);
        if (program.mMem)
            free(program.mMem);
    }
}

int Evaluators::GetMask(size_t nodeType)
{
    const std::string& nodeName = gMetaNodes[nodeType].mName;
#ifdef _DEBUG
    mEvaluatorPerNodeType[nodeType].mName = nodeName;
#endif
    int mask = 0;
    auto iter = mEvaluatorScripts.find(nodeName + ".glsl");
    if (iter != mEvaluatorScripts.end())
    {
        mask |= EvaluationGLSL;
        iter->second.mType = int(nodeType);
        mEvaluatorPerNodeType[nodeType].mGLSLProgram = iter->second.mProgram;
    }
    iter = mEvaluatorScripts.find(nodeName + ".glslc");
    if (iter != mEvaluatorScripts.end())
    {
        mask |= EvaluationGLSLCompute;
        iter->second.mType = int(nodeType);
        mEvaluatorPerNodeType[nodeType].mGLSLProgram = iter->second.mProgram;
    }
    iter = mEvaluatorScripts.find(nodeName + ".c");
    if (iter != mEvaluatorScripts.end())
    {
        mask |= EvaluationC;
        iter->second.mType = int(nodeType);
        mEvaluatorPerNodeType[nodeType].mCFunction = iter->second.mCFunction;
        mEvaluatorPerNodeType[nodeType].mMem = iter->second.mMem;
    }
    iter = mEvaluatorScripts.find(nodeName + ".py");
    if (iter != mEvaluatorScripts.end())
    {
        mask |= EvaluationPython;
        iter->second.mType = int(nodeType);
        mEvaluatorPerNodeType[nodeType].mPyModule = iter->second.mPyModule;
    }
    return mask;
}

void Evaluators::InitPythonModules()
{
    mImogenModule = pybind11::module::import("Imogen");
    mImogenModule.dec_ref();
}

void Evaluator::RunPython() const
{
    mPyModule.attr("main")(gEvaluators.mImogenModule.attr("accessor_api")());
}

