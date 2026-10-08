#include "cdlpch.h"
#include "Image.h"

namespace Candle {

	Image::Image(vk::Image image, vk::raii::ImageView imageView)
		: m_Image(std::move(image)), m_ImageView(std::move(imageView))
	{
	}

}