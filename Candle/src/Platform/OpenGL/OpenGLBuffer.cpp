#include "candlepch.h"
#include "OpenGLBuffer.h"

#include <glad/glad.h>

namespace Candle {

	//----------------------- VERTEX BUFFER ----------------------------------//

	OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, size_t size)
	{
		glCreateBuffers(1, &_renderId);
		glBindBuffer(GL_ARRAY_BUFFER, _renderId);
		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}

	OpenGLVertexBuffer::~OpenGLVertexBuffer()
	{
		glDeleteBuffers(1, &_renderId);
	}

	void OpenGLVertexBuffer::Bind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, _renderId);
	}

	void OpenGLVertexBuffer::Unbind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	//-----------------------------------------------------------------------//

	//----------------------- INDEX BUFFER ----------------------------------//

	OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count) : _count(count)
	{
		glCreateBuffers(1, &_renderId);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _renderId);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
	}

	OpenGLIndexBuffer::~OpenGLIndexBuffer()
	{
		glDeleteBuffers(1, &_renderId);
	}

	void OpenGLIndexBuffer::Bind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _renderId);
	}

	void OpenGLIndexBuffer::Unbind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	//-----------------------------------------------------------------------//
}