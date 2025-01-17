/*=============================================================================
 *
 *  Copyright (c) 2019 Sunnybrook Research Institute
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 *=============================================================================*/

#include "ColorMap.h"
#include <sstream>
#include <vector>

//Sedeen required headers
#include "Algorithm.h"
#include "Geometry.h"
#include "Global.h"
#include "Image.h"
#include "BindingsOpenCV.h"

// Poco header needed for the macros below 
#include <Poco/ClassLibrary.h>

// Declare that this object has AlgorithmBase subclasses
//  and declare each of those sub-classes
POCO_BEGIN_MANIFEST(sedeen::algorithm::AlgorithmBase)
POCO_EXPORT_CLASS(sedeen::algorithm::ColorMap)
POCO_END_MANIFEST

namespace sedeen {
namespace algorithm {

//Constructor
ColorMap::ColorMap() 
	:xscale_(1.0),
	yscale_(1.0),
	transparency_(),
	display_area_(),
	openFileDialogParam_(),
    displayColorMapOnly_(),
	display_result_(),
	selectedFileTobeProcessed_("") 
{
}//end constructor

//Destructor
ColorMap::~ColorMap() {
}//end destructor


void ColorMap::run() {

	auto display_area_isChanged = display_area_.isChanged();
	DisplayRegion display_region = display_area_;
	downsample_size_ = display_region.output_size;
	xscale_ = (double)(display_region.output_size.width()) / (double)display_region.source_region.width();
	yscale_ = (double)(display_region.output_size.height()) / (double)display_region.source_region.height();

	image::RawImage outputImage(downsample_size_, ColorSpace(ColorModel::RGBA, ChannelType::UInt8));
	outputImage.fill(0);
	if (parametersChanged() || display_area_isChanged) {
		creatHeatMap(outputImage);
		display_result_.update(outputImage, display_region.source_region);
	}
}//end run

void ColorMap::creatHeatMap(image::RawImage& outputImage)
{
	if (selectedFileTobeProcessed_.empty())
	{
		sedeen::algorithm::parameter::OpenFileDialog::DataType openFileDialogDataType = openFileDialogParam_;
		selectedFileTobeProcessed_ = openFileDialogDataType.at(0).getFilename();
		if (!openFileDialogParam_.isUserDefined() || selectedFileTobeProcessed_.empty())
		{
			/*msgBox.setText("Out put directory not set.");
			int ret = msgBox.exec();*/
			throw std::runtime_error("You did not select the correct file format!");
			//MessageBox(nullptr, LPCSTR("Please select a directory to save tiles!"), LPCSTR("Notification"), MB_OKCANCEL);
			return;
		}
	}

	std::string path_to_image =
		image()->getMetaData()->get(image::StringTags::SOURCE_DESCRIPTION, 0);

	auto source_factory = image()->getFactory();

	DisplayRegion display_region = display_area_;
	auto region = display_region.source_region;

	image::tile::Compositor compositor(source_factory);
	outputImage = compositor.getImage(display_region.source_region, display_region.output_size);

    //Since the slider is "transparency" and the input parameter is opacity, flip the value used
	float g_max_opacity = (100.0f - float(transparency_)) / 100.0f;  
	/*cv::Mat heatmap = cv::Mat::zeros(downsample_size_.height(), downsample_size_.width(), CV_32FC1);
	heatmap = meshGrid_.clone();
	meshGrid_= cv::Mat();*/

	cv::Mat meshGrid = cv::imread(selectedFileTobeProcessed_, CV_LOAD_IMAGE_GRAYSCALE);
	//cv::Mat heatmap = cv::Mat::zeros(downsample_size_.height(), downsample_size_.width(), CV_32FC1);
	cv::Mat heatmap, heatmap_n;
	cv::resize(meshGrid, heatmap, cv::Size(downsample_size_.width(), downsample_size_.height()));
	cv::normalize(heatmap, heatmap_n, 0.0, 1.0, cv::NORM_MINMAX, CV_32FC1);
	double tileSize = 10;
	double dummyValue = 1.5;
	cv::Mat temp_map;
	
    int kernel_size = (tileSize*xscale_ > 3.0) ? static_cast<int>(dummyValue*tileSize*xscale_) : 3;
	kernel_size = kernel_size%2 == 0 ? (kernel_size-1): kernel_size;    
    if(kernel_size >= tileSize )
	{
		kernel_size = static_cast<int>(tileSize)%2 == 0 ? static_cast<int>(tileSize + 1.0) : static_cast<int>(tileSize);
	}

	if (!heatmap_n.empty()) {
		cv::blur(heatmap_n, temp_map, cv::Size(kernel_size, kernel_size));
	}

	cv::Mat opencvMap, opencvColor;
	/*double min, max;
	cv::minMaxLoc(heatmap, &min, &max);*/
	cv::normalize(temp_map, opencvMap, 0, 255, cv::NORM_MINMAX, CV_8UC1);
	cv::applyColorMap(opencvMap, opencvColor, cv::COLORMAP_JET);

	double xshift = region.x()*xscale_;
	double yshift = region.y()*yscale_;
	image::Iterator it = getIterator(image(), region, downsample_size_);
	while (!it.finished())
	{
		auto position = Point(it.x(), it.y());
		int x = it.x();// -  xshift;
		int y = it.y();// -  yshift;

		if (region.contains(position) && (!heatmap_n.empty())) { //
			const float heat_mix = heatmap_n.at<float>(cv::Point(x, y));
            //If displayHeatmapOnly is true, mix between white and the color map value
            //Otherwise, get the pixel value in BGR element order
            cv::Vec3b i_color;
            if (displayColorMapOnly_ == true) {
                i_color = cv::Vec3b((unsigned char)(255),
                    (unsigned char)(255),
                    (unsigned char)(255));
            }
            else {
                i_color = cv::Vec3b((unsigned char)it.getComponent(2),
                    (unsigned char)it.getComponent(1),
                    (unsigned char)it.getComponent(0));
            }
			const cv::Vec3b heat_color = opencvColor.at<cv::Vec3b>(cv::Point(x, y));
			const float heat_mix2 = std::min(heat_mix, g_max_opacity);
			const cv::Vec3b final_color = interpolate(i_color, heat_color, heat_mix2);
			for (int j = 0; j < 3; j++) {
				outputImage.setValue(x, y, j, final_color[2 - j]);
			}
		}

		it.advance();
	}
}//end createHeatMap

void ColorMap::init(const image::ImageHandle& input_image) {
	if (isNull(input_image)) return;
	// Create system parameter - provide information about current view in UI
	display_area_ = createDisplayAreaParameter(*this);
	// Bind display result image to UI
	display_result_ = createImageResult(*this, "Final Image");

	transparency_ = createIntegerParameter( // use the creation function
		*this, // reference to the algorithm
		"Transparency", // name
		"Set the max transparency of the color map", // description
		40,  // default value
		0,   // minimum allowed value
		100, // maximum allowed value
		false); // optional or not

    //User can choose whether to show the color map only, or overlay it on the slide image
    displayColorMapOnly_ = createBoolParameter(*this, "Display color map only",
        "If this box is checked, only the color map will be displayed, and the transparency will be with respect to a white field.",
        false, false); //default value, optional

	file::FileDialogOptions fileDialogOptions;
	file::FileDialogFilter fileDialogFilter;
	fileDialogFilter.name = "Image (*.jpg)";
	fileDialogOptions.filters.push_back(fileDialogFilter);
	openFileDialogParam_ = sedeen::algorithm::createOpenFileDialogParameter(*this,
		"Intensity map file (.jpg)",
		"Choose a .jpg file containing an intensity or probability map to overlay on the whole-slide image",
		fileDialogOptions,
		false);

	xscale_ = 1.0;
	yscale_ = 1.0;
}

bool ColorMap::parametersChanged() {
	return ( transparency_.isChanged()
        || displayColorMapOnly_.isChanged() );
}

cv::Vec3b ColorMap::interpolate(const cv::Vec3b color1, const cv::Vec3b color2, const float value)
{
	uchar b = cv::saturate_cast<uchar>(round((1.0 - value)*color1.val[0] + value * color2.val[0]));
	uchar g = cv::saturate_cast<uchar>(round((1.0 - value)*color1.val[1] + value * color2.val[1]));
	uchar r = cv::saturate_cast<uchar>(round((1.0 - value)*color1.val[2] + value * color2.val[2]));

	return cv::Vec3b(b, g, r);
}

double ColorMap::round(double val)
{
	return floor(val + 0.5);
}

} // namespace algorithm
} // namespace sedeen