#pragma once

#include "Candle/Renderer/VertexArray.h"

namespace Candle {

    class OpenGLVertexArray : public VertexArray
    {
    public:
        OpenGLVertexArray();
        ~OpenGLVertexArray();

        virtual void Bind() const;
        virtual void Unbind() const;

        virtual void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer);
        virtual void AddIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer);

        inline virtual const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const { return _vertexBuffers; }
        inline virtual const std::shared_ptr<IndexBuffer>& GetIndexBuffers() const { return _indexBuffer; }

    private:
        uint32_t _renderId;

        std::vector<std::shared_ptr<VertexBuffer>> _vertexBuffers;
        std::shared_ptr<IndexBuffer> _indexBuffer;
    };
}

