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

#pragma once

#include <vector>
#include <string>
#include "imgui.h"
#include "imgui_internal.h"
#include "Library.h"

struct TileNodeEditGraphDelegate;
struct Evaluation;
class TextEditor;
struct Library;


enum EVALUATOR_TYPE
{
	EVALUATOR_GLSL,
	EVALUATOR_C,
	EVALUATOR_PYTHON,
};

struct EvaluatorFile
{
	std::string mDirectory;
	std::string mFilename;
	EVALUATOR_TYPE mEvaluatorType;
};

struct Imogen
{
	Imogen();
	~Imogen();

	void Init();
	void Finish();
	
	void Show(Library& library, TileNodeEditGraphDelegate &nodeGraphDelegate, Evaluation& evaluation);
	void ValidateCurrentMaterial(Library& library, TileNodeEditGraphDelegate &nodeGraphDelegate);
	void DiscoverNodes(const char *extension, const char *directory, EVALUATOR_TYPE evaluatorType, std::vector<EvaluatorFile>& files);

	std::vector<EvaluatorFile> mEvaluatorFiles;
	int GetCurrentMaterialIndex();

protected:
	void HandleEditor(TextEditor &editor, TileNodeEditGraphDelegate &nodeGraphDelegate, Evaluation& evaluation);
};

void DebugLogText(const char *szText);
enum CallbackDisplayType
{
	CBUI_Node,
	CBUI_Progress,
	CBUI_Cubemap
};
struct ImogenDrawCallback
{
	CallbackDisplayType mType;
	ImRect mClippedRect;
	ImRect mOrginalRect;
	size_t mNodeIndex;
};
extern std::vector<ImogenDrawCallback> mCallbackRects;
void InitCallbackRects();
size_t AddNodeUICallbackRect(CallbackDisplayType type, const ImRect& rect, size_t nodeIndex);
extern int gEvaluationTime;

struct UndoRedo
{
	virtual ~UndoRedo() {}
	virtual void Undo() = 0;
	virtual void Redo() = 0;
};

struct UndoRedoParameterBlock : public UndoRedo
{
	UndoRedoParameterBlock(size_t target, const std::vector<unsigned char>& preDo) : mTarget(target), mPreDo(preDo) {}
	virtual ~UndoRedoParameterBlock() {}
	virtual void Undo();
	virtual void Redo();
	std::vector<unsigned char> mPreDo;
	std::vector<unsigned char> mPostDo;
	size_t mTarget;
};

struct UndoRedoInputSampler : public UndoRedo
{
	UndoRedoInputSampler(size_t target, const std::vector<InputSampler>& preDo) : mTarget(target), mPreDo(preDo) {}
	virtual ~UndoRedoInputSampler() {}
	virtual void Undo();
	virtual void Redo();
	std::vector<InputSampler> mPreDo;
	std::vector<InputSampler> mPostDo;
	size_t mTarget;
};


struct UndoRedoNodeLinks : public UndoRedo
{
	UndoRedoNodeLinks(size_t linkIndex, const std::vector<NodeLink>& preDo) : mLinkIndex(linkIndex), mPreDo(preDo) {}
	virtual ~UndoRedoNodeLinks() {}
	virtual void Undo();
	virtual void Redo();
	std::vector<NodeLink> mPreDo;
	std::vector<NodeLink> mPostDo;
	size_t mLinkIndex;
};

struct UndoRedoHandler
{
	~UndoRedoHandler()
	{
		Clear();
	}

	void Undo()
	{
		if (mUndos.empty())
			return;
		mUndos.back()->Undo();
		mRedos.push_back(mUndos.back());
		mUndos.pop_back();
	}

	void Redo()
	{
		if (mRedos.empty())
			return;
		mRedos.back()->Redo();
		mUndos.push_back(mRedos.back());
		mRedos.pop_back();
	}

	template <typename T> void AddUndo(const T &undoRedo)
	{
		mUndos.push_back(new T(undoRedo));
		for (auto redo : mRedos)
			delete redo;
		mRedos.clear();
	}


	void Clear()
	{
		for (auto undo : mUndos)
			delete undo;
		mUndos.clear();

		for (auto redo : mRedos)
			delete redo;
		mRedos.clear();
	}

private:
	std::vector<UndoRedo *> mUndos;
	std::vector<UndoRedo *> mRedos;
};

extern UndoRedoHandler gUndoRedoHandler;
