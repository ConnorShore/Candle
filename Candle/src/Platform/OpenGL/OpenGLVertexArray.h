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

        virtual void AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer);
        virtual void AddIndexBuffer(const Ref<IndexBuffer>& indexBuffer);

        inline virtual const std::vector<Ref<VertexBuffer>>& GetVertexBuffers() const { return _vertexBuffers; }
        inline virtual const Ref<IndexBuffer>& GetIndexBuffers() const { return _indexBuffer; }

    private:
        uint32_t _renderId;

        std::vector<Ref<VertexBuffer>> _vertexBuffers;
        Ref<IndexBuffer> _indexBuffer;
    };
}

