/*
 * Copyright (c) 2017-2021 Arm Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
//Ehsan
#include "arm_compute/gl_vs.h"
#ifndef My_print
#include "arm_compute/gl_vs.h"
#endif

arm_compute::graph::Tensor *f_out;
arm_compute::graph::Tensor *s_in;
arm_compute::graph::Tensor *s_out;
arm_compute::graph::Tensor *t_in;


#include <mutex>
#include <condition_variable>
#include <queue>

std::mutex m1;
std::condition_variable cv1;
std::mutex m2;
std::condition_variable cv2;

static bool waiting1 = true;
static bool data_ready1 = false;
static bool waiting2 = true;
static bool data_ready2 = false;

#include "arm_compute/runtime/Tensor.h"

//static std::queue<std::shared_ptr<arm_compute::ITensor>> Tensors_Q;
arm_compute::Tensor transit_tensor1;
arm_compute::Tensor transit_tensor2;

static std::queue<arm_compute::Tensor*> Tensors_Q1;
static std::queue<arm_compute::Tensor*> Tensors_Q2;


#include "utils/GraphUtils.h"

#include "arm_compute/core/Helpers.h"
#include "arm_compute/core/Types.h"
#include "arm_compute/graph/Logger.h"
#include "arm_compute/runtime/SubTensor.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "utils/ImageLoader.h"
#pragma GCC diagnostic pop
#include "utils/Utils.h"

#include <inttypes.h>
#include <iomanip>
#include <limits>

using namespace arm_compute::graph_utils;

namespace
{
std::pair<arm_compute::TensorShape, arm_compute::PermutationVector> compute_permutation_parameters(const arm_compute::TensorShape &shape,
                                                                                                   arm_compute::DataLayout data_layout)
{
    // Set permutation parameters if needed
    arm_compute::TensorShape       permuted_shape = shape;
    arm_compute::PermutationVector perm;
    // Permute only if num_dimensions greater than 2
    if(shape.num_dimensions() > 2)
    {
        perm = (data_layout == arm_compute::DataLayout::NHWC) ? arm_compute::PermutationVector(2U, 0U, 1U) : arm_compute::PermutationVector(1U, 2U, 0U);

        arm_compute::PermutationVector perm_shape = (data_layout == arm_compute::DataLayout::NCHW) ? arm_compute::PermutationVector(2U, 0U, 1U) : arm_compute::PermutationVector(1U, 2U, 0U);
        arm_compute::permute(permuted_shape, perm_shape);
    }

    return std::make_pair(permuted_shape, perm);
}
} // namespace

TFPreproccessor::TFPreproccessor(float min_range, float max_range)
    : _min_range(min_range), _max_range(max_range)
{
}
void TFPreproccessor::preprocess(ITensor &tensor)
{
    if(tensor.info()->data_type() == DataType::F32)
    {
        preprocess_typed<float>(tensor);
    }
    else if(tensor.info()->data_type() == DataType::F16)
    {
        preprocess_typed<half>(tensor);
    }
    else
    {
        ARM_COMPUTE_ERROR("NOT SUPPORTED!");
    }
}

template <typename T>
void TFPreproccessor::preprocess_typed(ITensor &tensor)
{
    Window window;
    window.use_tensor_dimensions(tensor.info()->tensor_shape());

    const float range = _max_range - _min_range;
    execute_window_loop(window, [&](const Coordinates & id)
    {
        const T value                                     = *reinterpret_cast<T *>(tensor.ptr_to_element(id));
        float   res                                       = value / 255.f;            // Normalize to [0, 1]
        res                                               = res * range + _min_range; // Map to [min_range, max_range]
        *reinterpret_cast<T *>(tensor.ptr_to_element(id)) = res;
    });
}

CaffePreproccessor::CaffePreproccessor(std::array<float, 3> mean, bool bgr, float scale)
    : _mean(mean), _bgr(bgr), _scale(scale)
{
    if(_bgr)
    {
        std::swap(_mean[0], _mean[2]);
    }
}

void CaffePreproccessor::preprocess(ITensor &tensor)
{
    if(tensor.info()->data_type() == DataType::F32)
    {
        preprocess_typed<float>(tensor);
    }
    else if(tensor.info()->data_type() == DataType::F16)
    {
        preprocess_typed<half>(tensor);
    }
    else
    {
        ARM_COMPUTE_ERROR("NOT SUPPORTED!");
    }
}

template <typename T>
void CaffePreproccessor::preprocess_typed(ITensor &tensor)
{
    Window window;
    window.use_tensor_dimensions(tensor.info()->tensor_shape());
    const int channel_idx = get_data_layout_dimension_index(tensor.info()->data_layout(), DataLayoutDimension::CHANNEL);
    execute_window_loop(window, [&](const Coordinates & id)
    {
        const T value                                     = *reinterpret_cast<T *>(tensor.ptr_to_element(id)) - T(_mean[id[channel_idx]]);
        *reinterpret_cast<T *>(tensor.ptr_to_element(id)) = value * T(_scale);

        //Ehsan
        //std::cout<<\nInput image\n<<"id:"<<id<<" v:"<<value<<std::endl;
    });
}

PPMWriter::PPMWriter(std::string name, unsigned int maximum)
    : _name(std::move(name)), _iterator(0), _maximum(maximum)
{
}

bool PPMWriter::access_tensor(ITensor &tensor)
{
    std::stringstream ss;
    ss << _name << _iterator << ".ppm";

    arm_compute::utils::save_to_ppm(tensor, ss.str());

    _iterator++;
    if(_maximum == 0)
    {
        return true;
    }
    return _iterator < _maximum;
}

DummyAccessor::DummyAccessor(bool type, unsigned int maximum)
    : _iterator(0), _maximum(maximum), _type(type)
{
}


bool DummyAccessor::access_tensor(ITensor &tensor)
{
	//std::cout<<"hhhh:"<<s_in->desc().shape<<std::endl;
	//Ehsan
	//First_NEON
	//tensor.copy_from(f_out->handle()->tensor());
	return _type;

    ARM_COMPUTE_UNUSED(tensor);
    bool ret = _maximum == 0 || _iterator < _maximum;
    if(_iterator == _maximum)
    {
        _iterator = 0;
    }
    else
    {
        _iterator++;
    }
    return ret;
}

TransferAccessor::TransferAccessor(bool tran, unsigned int maximum)
	: transition(tran)//_iterator(0), _maximum(maximum),
{
	//std::cout<<"\ntransferaccessor1\n";
	transit_tensor1.allocator()->init(*(f_out->handle()->tensor().info()));
	transit_tensor1.allocator()->allocate();
}
TransferAccessor2::TransferAccessor2(bool tran, unsigned int maximum)
	:  transition(tran)// _iterator(0), _maximum(maximum),
{
	//std::cout<<"\ntransferaccessor2\n";
	transit_tensor2.allocator()->init(*(s_out->handle()->tensor().info()));
	transit_tensor2.allocator()->allocate();
}


//input of second graph
bool TransferAccessor::access_tensor(ITensor &tensor)
{
	//std::cout<<"second graph receiver is waiting for mutex\n";
	std::unique_lock<std::mutex> lk(m1);
	if (Tensors_Q1.empty()){
		//std::cout<<"q of second graph is empty waits\n";
		//std::cout<<"\nQ is empty waiting for source data\n";
		waiting1 = 1;
		cv1.wait(lk, []{return data_ready1;});
		//std::cout<<"second graph receiver wake up\n";
		data_ready1=0;
		waiting1=0;
		lk.unlock();

#if My_print > 0
		std::cout<<"\nrecieving data from first graph\n";
#endif
		//if(s_in->desc().target==arm_compute::graph::Target ::CL )
		if(transition)
		{
			//std::cout<<"second graph receiver is responsible for transfering\n";
			//std::cout<<"\n transfering data from source directly transfer is done in dest\n";
			auto tstart=std::chrono::high_resolution_clock::now();
			tensor.copy_from(f_out->handle()->tensor());
			auto tfinish=std::chrono::high_resolution_clock::now();
			double cost0 = std::chrono::duration_cast<std::chrono::duration<double>>(tfinish - tstart).count();
#if My_print > 0
			std::cout<<"\nTransfer time from source:"<<cost0<<std::endl<<std::endl;
#endif
		}

#if My_print > 0
		std::cout<<"\nReceived\n";
#endif
	}
	else{
		//std::cout<<"\nTransfer from Queue at dest\n";
		//std::cout<<"second graph is reading from q\n";
		auto tstart=std::chrono::high_resolution_clock::now();
		tensor.copy_from(*Tensors_Q1.front());
		//tensor.copy_from(Tensors_Q.front().handle()->tensor());
		Tensors_Q1.pop();
		auto tfinish=std::chrono::high_resolution_clock::now();
		double cost0 = std::chrono::duration_cast<std::chrono::duration<double>>(tfinish - tstart).count();
#if My_print > 0
		std::cout<<"\nTransfer time from queue:"<<cost0<<std::endl<<std::endl;
#endif
		lk.unlock();
	}
	//std::cout<<"second graph is ready to run nodes\n";
	return true;



/*

	bool ret = _maximum == 0 || _iterator < _maximum;
	if(_iterator == _maximum)
	{
		_iterator = 0;
	}
	else
	{
		_iterator++;
	}

	if(!ret)
		return ret;




#if My_print > 0
	std::cout<<"\nrecieving data from first graph\n";
#endif
	//if(s_in->desc().target==arm_compute::graph::Target ::CL)
	if(transition)
	{
		auto tstart=std::chrono::high_resolution_clock::now();
		tensor.copy_from(f_out->handle()->tensor());
		auto tfinish=std::chrono::high_resolution_clock::now();
		double cost0 = std::chrono::duration_cast<std::chrono::duration<double>>(tfinish - tstart).count();
		std::cout<<"\nTransfer time:"<<cost0<<std::endl<<std::endl;
	}
#if My_print > 0
	std::cout<<"\nReceived\n";
#endif

	//return ret;
	return true;

	*/

}


//input of third graph
bool TransferAccessor2::access_tensor(ITensor &tensor)
{
	//std::cout<<"third graph receiver is waiting for mutex\n";
	std::unique_lock<std::mutex> lk(m2);
	if (Tensors_Q2.empty()){
		//std::cout<<"q of third graph is empty want to wait\n";
		//std::cout<<"\nQ is empty waiting for source data\n";
		waiting2 = 1;
		cv2.wait(lk, []{return data_ready2;});
		data_ready2=0;
		waiting2=0;
		lk.unlock();
		//std::cout<<"third garaph receiver wake up\n";

#if My_print > 0
		std::cout<<"\nrecieving data from second graph\n";
#endif
		//if(t_in->desc().target==arm_compute::graph::Target ::CL)
		if(transition)
		{
			//std::cout<<"third graph is responsible for transfer\n";
			//std::cout<<"\n transfering data from source directly transfer is done in dest\n";
			auto tstart=std::chrono::high_resolution_clock::now();
			tensor.copy_from(s_out->handle()->tensor());
			auto tfinish=std::chrono::high_resolution_clock::now();
			//std::cout<<"third graph transfered data\n";
			double cost0 = std::chrono::duration_cast<std::chrono::duration<double>>(tfinish - tstart).count();
#if My_print > 0
			std::cout<<"\nTransfer time from source:"<<cost0<<std::endl<<std::endl;
#endif
		}
#if My_print > 0
		std::cout<<"\nReceived\n";
#endif
	}
	else{
		//std::cout<<"third graph want to pull from q\n";
		//std::cout<<"\nTransfer from Queue at dest\n";
		auto tstart=std::chrono::high_resolution_clock::now();
		tensor.copy_from(*Tensors_Q2.front());
		//tensor.copy_from(Tensors_Q.front().handle()->tensor());
		Tensors_Q2.pop();
		auto tfinish=std::chrono::high_resolution_clock::now();
		double cost0 = std::chrono::duration_cast<std::chrono::duration<double>>(tfinish - tstart).count();
#if My_print > 0
		std::cout<<"\nTransfer time from queue:"<<cost0<<std::endl<<std::endl;
#endif
		//std::cout<<"third graph pulled from q\n";
		lk.unlock();
	}
	//std::cout<<"third graph is ready to run tasks\n";
	return true;



/*

	bool ret = _maximum == 0 || _iterator < _maximum;
	if(_iterator == _maximum)
	{
		_iterator = 0;
	}
	else
	{
		_iterator++;
	}

	if(!ret)
		return ret;




#if My_print > 0
	std::cout<<"\nrecieving data from first graph\n";
#endif
	//if(t_in->desc().target==arm_compute::graph::Target ::CL)
	if(transition)
	{
		auto tstart=std::chrono::high_resolution_clock::now();
		tensor.copy_from(s_out->handle()->tensor());
		auto tfinish=std::chrono::high_resolution_clock::now();
		double cost0 = std::chrono::duration_cast<std::chrono::duration<double>>(tfinish - tstart).count();
		std::cout<<"\nTransfer time:"<<cost0<<std::endl<<std::endl;
	}
#if My_print > 0
	std::cout<<"\nReceived\n";
#endif

	//return ret;
	return true;

	*/




	//std::cout<<"hhhh:"<<s_in->desc().shape<<std::endl;
	//Ehsan
	//First_NEON

	/*bool ret = _maximum == 0 || _iterator < _maximum;
	if(_iterator == _maximum)
	{
		_iterator = 0;
	}
	else
	{
		_iterator++;
	}

	if(!ret)
		return ret;
	*/
}









MySaveAccessor::MySaveAccessor(const std::string npy_name, const bool is_fortran , unsigned int maximum)
    : _iterator(0), _maximum(maximum), _npy_name(std::move(npy_name)), _is_fortran(is_fortran)
{
}

#include <filesystem>
#include <iostream>

bool MySaveAccessor::access_tensor(ITensor &tensor)
{
	//std::cout<<"hhhh:"<<s_in->desc().shape<<std::endl;
	//Ehsan
	//First_NEON
	//tensor.copy_from(f_out->handle()->tensor());



	if(!saved){



		//Generate and fill with random numbers
		std::uniform_real_distribution<float> distribution_f32(0.0, 255.0);
		std::random_device::result_type _seed=0;
	    std::mt19937 gen(_seed);

	    if(tensor.info()->padding().empty() && (dynamic_cast<SubTensor *>(&tensor) == nullptr))
	    {
	        for(size_t offset = 0; offset < tensor.info()->total_size(); offset += tensor.info()->element_size())
	        {
	            const auto value                                 = static_cast<float>(distribution_f32(gen));
	            *reinterpret_cast<float *>(tensor.buffer() + offset) = value;
	        }
	    }
	    else
	    {
	        // If tensor has padding accessing tensor elements through execution window.
	        Window window;
	        window.use_tensor_dimensions(tensor.info()->tensor_shape());

	        execute_window_loop(window, [&](const Coordinates & id)
	        {
	            const auto value                                  = static_cast<float>(distribution_f32(gen));
	            *reinterpret_cast<float *>(tensor.ptr_to_element(id)) = value;
	        });
	    }




	    auto len=_npy_name.length();
	    auto index=_npy_name.find_last_of('/');
	    bool path=true;
	    if (index == std::string::npos) {
	        path=false;
	    }

	    if(path){
	    	std::system(("mkdir -p "+_npy_name.substr(0,index)).c_str());
	    	std::cout<<"create path:"<<_npy_name.substr(0,index)<<std::endl;

	    }
	    std::cout<<"file name:"<<_npy_name.substr(len-index,len-1)<<std::endl;
	    //std::string tes;
	    //std::cout<<"press to continue...";
	    //std::cin>>tes;
	    //std::cout<<std::endl;
		utils::save_to_npy(tensor, _npy_name, _is_fortran);
		saved=true;
	}


    ARM_COMPUTE_UNUSED(tensor);
    bool ret = _maximum == 0 || _iterator < _maximum;
    if(_iterator == _maximum)
    {
        _iterator = 0;
    }
    else
    {
        _iterator++;
    }
    return ret;
}






NumPyAccessor::NumPyAccessor(std::string npy_path, TensorShape shape, DataType data_type, DataLayout data_layout, std::ostream &output_stream)
    : _npy_tensor(), _filename(std::move(npy_path)), _output_stream(output_stream)
{
    NumPyBinLoader loader(_filename, data_layout);

    TensorInfo info(shape, 1, data_type);
    info.set_data_layout(data_layout);

    _npy_tensor.allocator()->init(info);
    _npy_tensor.allocator()->allocate();

    loader.access_tensor(_npy_tensor);
}

template <typename T>
void NumPyAccessor::access_numpy_tensor(ITensor &tensor, T tolerance)
{
    const int num_elements          = tensor.info()->tensor_shape().total_size();
    int       num_mismatches        = utils::compare_tensor<T>(tensor, _npy_tensor, tolerance);
    float     percentage_mismatches = static_cast<float>(num_mismatches) / num_elements;

    _output_stream << "Results: " << 100.f - (percentage_mismatches * 100) << " % matches with the provided output[" << _filename << "]." << std::endl;
    _output_stream << "         " << num_elements - num_mismatches << " out of " << num_elements << " matches with the provided output[" << _filename << "]." << std::endl
                   << std::endl;
}

bool NumPyAccessor::access_tensor(ITensor &tensor)
{
    ARM_COMPUTE_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(&tensor, 1, DataType::F32, DataType::QASYMM8);
    ARM_COMPUTE_ERROR_ON(_npy_tensor.info()->dimension(0) != tensor.info()->dimension(0));

    switch(tensor.info()->data_type())
    {
        case DataType::QASYMM8:
            access_numpy_tensor<qasymm8_t>(tensor, 0);
            break;
        case DataType::F32:
            access_numpy_tensor<float>(tensor, 0.0001f);
            break;
        default:
            ARM_COMPUTE_ERROR("NOT SUPPORTED!");
    }

    return false;
}

#ifdef ARM_COMPUTE_ASSERTS_ENABLED
PrintAccessor::PrintAccessor(std::ostream &output_stream, IOFormatInfo io_fmt)
    : _output_stream(output_stream), _io_fmt(io_fmt)
{
}

bool PrintAccessor::access_tensor(ITensor &tensor)
{
    tensor.print(_output_stream, _io_fmt);
    return false;
}
#endif /* ARM_COMPUTE_ASSERTS_ENABLED */

SaveNumPyAccessor::SaveNumPyAccessor(std::string npy_name, const bool is_fortran)
    : _npy_name(std::move(npy_name)), _is_fortran(is_fortran)
{
}

bool SaveNumPyAccessor::access_tensor(ITensor &tensor)
{
    ARM_COMPUTE_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(&tensor, 1, DataType::F32);

    utils::save_to_npy(tensor, _npy_name, _is_fortran);

    return false;
}

ImageAccessor::ImageAccessor(std::string filename, bool bgr, std::unique_ptr<IPreprocessor> preprocessor)
    : _already_loaded(false), _filename(std::move(filename)), _bgr(bgr), _preprocessor(std::move(preprocessor))
{
}


//Ehsan
bool ImageAccessor::set_filename(std::string filename){
	_filename=filename;
	_already_loaded=false;
	return _already_loaded;
}

bool ImageAccessor::access_tensor(ITensor &tensor)
{

    if(!_already_loaded)
    {
	//Ehsan
        ////std::cout<<"\n\n\n\naccess_tensor is called!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!******************\n\n\n";

        auto image_loader = utils::ImageLoaderFactory::create(_filename);
        ARM_COMPUTE_EXIT_ON_MSG(image_loader == nullptr, "Unsupported image type");

        // Open image file
        image_loader->open(_filename);

        // Get permutated shape and permutation parameters
        TensorShape                    permuted_shape = tensor.info()->tensor_shape();
        arm_compute::PermutationVector perm;
        if(tensor.info()->data_layout() != DataLayout::NCHW)
        {
            std::tie(permuted_shape, perm) = compute_permutation_parameters(tensor.info()->tensor_shape(), tensor.info()->data_layout());
        }

#ifdef __arm__
        ARM_COMPUTE_EXIT_ON_MSG_VAR(image_loader->width() != permuted_shape.x() || image_loader->height() != permuted_shape.y(),
                                    "Failed to load image file: dimensions [%d,%d] not correct, expected [%" PRIu32 ",%" PRIu32 "].",
                                    image_loader->width(), image_loader->height(), permuted_shape.x(), permuted_shape.y());
#else  // __arm__
        ARM_COMPUTE_EXIT_ON_MSG_VAR(image_loader->width() != permuted_shape.x() || image_loader->height() != permuted_shape.y(),
                                    "Failed to load image file: dimensions [%d,%d] not correct, expected [%" PRIu64 ",%" PRIu64 "].",
                                    image_loader->width(), image_loader->height(),
                                    static_cast<uint64_t>(permuted_shape.x()), static_cast<uint64_t>(permuted_shape.y()));
#endif // __arm__

        // Fill the tensor with the PPM content (BGR)
        image_loader->fill_planar_tensor(tensor, _bgr);

        // Preprocess tensor
        if(_preprocessor)
        {
            _preprocessor->preprocess(tensor);
        }
    }

    //Ehsan
    //_already_loaded=false;
    _already_loaded = !_already_loaded;
    return _already_loaded;


    //return true;
}

ValidationInputAccessor::ValidationInputAccessor(const std::string             &image_list,
                                                 std::string                    images_path,
                                                 std::unique_ptr<IPreprocessor> preprocessor,
                                                 bool                           bgr,
                                                 unsigned int                   start,
                                                 unsigned int                   end,
                                                 std::ostream                  &output_stream)
    : _path(std::move(images_path)), _images(), _preprocessor(std::move(preprocessor)), _bgr(bgr), _offset(0), _output_stream(output_stream)
{
    ARM_COMPUTE_EXIT_ON_MSG(start > end, "Invalid validation range!");

    std::ifstream ifs;
    try
    {
        ifs.exceptions(std::ifstream::badbit);
        ifs.open(image_list, std::ios::in | std::ios::binary);

        // Parse image names
        unsigned int counter = 0;
        for(std::string line; !std::getline(ifs, line).fail() && counter <= end; ++counter)
        {
            // Add image to process if withing range
            if(counter >= start)
            {
                std::stringstream linestream(line);
                std::string       image_name;

                linestream >> image_name;
                _images.emplace_back(std::move(image_name));
            }
        }
    }
    catch(const std::ifstream::failure &e)
    {
        ARM_COMPUTE_ERROR_VAR("Accessing %s: %s", image_list.c_str(), e.what());
    }
}

bool ValidationInputAccessor::access_tensor(arm_compute::ITensor &tensor)
{
    bool ret = _offset < _images.size();
    if(ret)
    {
        utils::JPEGLoader jpeg;

        // Open JPEG file
        std::string image_name = _path + _images[_offset++];
        jpeg.open(image_name);
        _output_stream << "[" << _offset << "/" << _images.size() << "] Validating " << image_name << std::endl;

        // Get permutated shape and permutation parameters
        TensorShape                    permuted_shape = tensor.info()->tensor_shape();
        arm_compute::PermutationVector perm;
        if(tensor.info()->data_layout() != DataLayout::NCHW)
        {
            std::tie(permuted_shape, perm) = compute_permutation_parameters(tensor.info()->tensor_shape(),
                                                                            tensor.info()->data_layout());
        }

#ifdef __arm__
        ARM_COMPUTE_EXIT_ON_MSG_VAR(jpeg.width() != permuted_shape.x() || jpeg.height() != permuted_shape.y(),
                                    "Failed to load image file: dimensions [%d,%d] not correct, expected [%" PRIu32 ",%" PRIu32 "].",
                                    jpeg.width(), jpeg.height(), permuted_shape.x(), permuted_shape.y());
#else  // __arm__
        ARM_COMPUTE_EXIT_ON_MSG_VAR(jpeg.width() != permuted_shape.x() || jpeg.height() != permuted_shape.y(),
                                    "Failed to load image file: dimensions [%d,%d] not correct, expected [%" PRIu64 ",%" PRIu64 "].",
                                    jpeg.width(), jpeg.height(),
                                    static_cast<uint64_t>(permuted_shape.x()), static_cast<uint64_t>(permuted_shape.y()));
#endif // __arm__

        // Fill the tensor with the JPEG content (BGR)
        jpeg.fill_planar_tensor(tensor, _bgr);

        // Preprocess tensor
        if(_preprocessor)
        {
            _preprocessor->preprocess(tensor);
        }
    }

    return ret;
}

ValidationOutputAccessor::ValidationOutputAccessor(const std::string &image_list,
                                                   std::ostream      &output_stream,
                                                   unsigned int       start,
                                                   unsigned int       end)
    : _results(), _output_stream(output_stream), _offset(0), _positive_samples_top1(0), _positive_samples_top5(0)
{
    ARM_COMPUTE_EXIT_ON_MSG(start > end, "Invalid validation range!");

    std::ifstream ifs;
    try
    {
        ifs.exceptions(std::ifstream::badbit);
        ifs.open(image_list, std::ios::in | std::ios::binary);

        // Parse image correctly classified labels
        unsigned int counter = 0;
        for(std::string line; !std::getline(ifs, line).fail() && counter <= end; ++counter)
        {
            // Add label if within range
            if(counter >= start)
            {
                std::stringstream linestream(line);
                std::string       image_name;
                int               result;

                linestream >> image_name >> result;
                _results.emplace_back(result);
            }
        }
    }
    catch(const std::ifstream::failure &e)
    {
        ARM_COMPUTE_ERROR_VAR("Accessing %s: %s", image_list.c_str(), e.what());
    }
}

void ValidationOutputAccessor::reset()
{
    _offset                = 0;
    _positive_samples_top1 = 0;
    _positive_samples_top5 = 0;
}

bool ValidationOutputAccessor::access_tensor(arm_compute::ITensor &tensor)
{
    bool ret = _offset < _results.size();
    if(ret)
    {
        // Get results
        std::vector<size_t> tensor_results;
        switch(tensor.info()->data_type())
        {
            case DataType::QASYMM8:
                tensor_results = access_predictions_tensor<uint8_t>(tensor);
                break;
            case DataType::F16:
                tensor_results = access_predictions_tensor<half>(tensor);
                break;
            case DataType::F32:
                tensor_results = access_predictions_tensor<float>(tensor);
                break;
            default:
                ARM_COMPUTE_ERROR("NOT SUPPORTED!");
        }

        // Check if tensor results are within top-n accuracy
        size_t correct_label = _results[_offset++];

        aggregate_sample(tensor_results, _positive_samples_top1, 1, correct_label);
        aggregate_sample(tensor_results, _positive_samples_top5, 5, correct_label);
    }

    // Report top_n accuracy
    if(_offset >= _results.size())
    {
        report_top_n(1, _results.size(), _positive_samples_top1);
        report_top_n(5, _results.size(), _positive_samples_top5);
    }

    return ret;
}

template <typename T>
std::vector<size_t> ValidationOutputAccessor::access_predictions_tensor(arm_compute::ITensor &tensor)
{
    // Get the predicted class
    std::vector<size_t> index;

    const auto   output_net  = reinterpret_cast<T *>(tensor.buffer() + tensor.info()->offset_first_element_in_bytes());
    const size_t num_classes = tensor.info()->dimension(0);

    index.resize(num_classes);

    // Sort results
    std::iota(std::begin(index), std::end(index), static_cast<size_t>(0));
    std::sort(std::begin(index), std::end(index),
              [&](size_t a, size_t b)
    {
        return output_net[a] > output_net[b];
    });

    return index;
}

void ValidationOutputAccessor::aggregate_sample(const std::vector<size_t> &res, size_t &positive_samples, size_t top_n, size_t correct_label)
{
    auto is_valid_label = [correct_label](size_t label)
    {
        return label == correct_label;
    };

    if(std::any_of(std::begin(res), std::begin(res) + top_n, is_valid_label))
    {
        ++positive_samples;
    }
}

void ValidationOutputAccessor::report_top_n(size_t top_n, size_t total_samples, size_t positive_samples)
{
    size_t negative_samples = total_samples - positive_samples;
    float  accuracy         = positive_samples / static_cast<float>(total_samples);

    _output_stream << "----------Top " << top_n << " accuracy ----------" << std::endl
                   << std::endl;
    _output_stream << "Positive samples : " << positive_samples << std::endl;
    _output_stream << "Negative samples : " << negative_samples << std::endl;
    _output_stream << "Accuracy : " << accuracy << std::endl;
}

DetectionOutputAccessor::DetectionOutputAccessor(const std::string &labels_path, std::vector<TensorShape> &imgs_tensor_shapes, std::ostream &output_stream)
    : _labels(), _tensor_shapes(std::move(imgs_tensor_shapes)), _output_stream(output_stream)
{
    _labels.clear();

    std::ifstream ifs;

    try
    {
        ifs.exceptions(std::ifstream::badbit);
        ifs.open(labels_path, std::ios::in | std::ios::binary);

        for(std::string line; !std::getline(ifs, line).fail();)
        {
            _labels.emplace_back(line);
        }
    }
    catch(const std::ifstream::failure &e)
    {
        ARM_COMPUTE_ERROR_VAR("Accessing %s: %s", labels_path.c_str(), e.what());
    }
}

template <typename T>
void DetectionOutputAccessor::access_predictions_tensor(ITensor &tensor)
{
    const size_t num_detection = tensor.info()->valid_region().shape.y();
    const auto   output_prt    = reinterpret_cast<T *>(tensor.buffer() + tensor.info()->offset_first_element_in_bytes());

    if(num_detection > 0)
    {
        _output_stream << "---------------------- Detections ----------------------" << std::endl
                       << std::endl;

        _output_stream << std::left << std::setprecision(4) << std::setw(8) << "Image | " << std::setw(8) << "Label | " << std::setw(12) << "Confidence | "
                       << "[ xmin, ymin, xmax, ymax ]" << std::endl;

        for(size_t i = 0; i < num_detection; ++i)
        {
            auto im = static_cast<const int>(output_prt[i * 7]);
            _output_stream << std::setw(8) << im << std::setw(8)
                           << _labels[output_prt[i * 7 + 1]] << std::setw(12) << output_prt[i * 7 + 2]
                           << " [" << (output_prt[i * 7 + 3] * _tensor_shapes[im].x())
                           << ", " << (output_prt[i * 7 + 4] * _tensor_shapes[im].y())
                           << ", " << (output_prt[i * 7 + 5] * _tensor_shapes[im].x())
                           << ", " << (output_prt[i * 7 + 6] * _tensor_shapes[im].y())
                           << "]" << std::endl;
        }
    }
    else
    {
        _output_stream << "No detection found." << std::endl;
    }
}

bool DetectionOutputAccessor::access_tensor(ITensor &tensor)
{
    ARM_COMPUTE_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(&tensor, 1, DataType::F32);

    switch(tensor.info()->data_type())
    {
        case DataType::F32:
            access_predictions_tensor<float>(tensor);
            break;
        default:
            ARM_COMPUTE_ERROR("NOT SUPPORTED!");
    }

    return false;
}

TopNPredictionsAccessor::TopNPredictionsAccessor(const std::string &labels_path, size_t top_n, std::ostream &output_stream)
    : _labels(), _output_stream(output_stream), _top_n(top_n)
{
    _labels.clear();

    std::ifstream ifs;

    try
    {
        ifs.exceptions(std::ifstream::badbit);
        ifs.open(labels_path, std::ios::in | std::ios::binary);

        for(std::string line; !std::getline(ifs, line).fail();)
        {
            _labels.emplace_back(line);
        }
    }
    catch(const std::ifstream::failure &e)
    {
        ARM_COMPUTE_ERROR_VAR("Accessing %s: %s", labels_path.c_str(), e.what());
    }
}

template <typename T>
void TopNPredictionsAccessor::access_predictions_tensor(ITensor &tensor)
{

    // Get the predicted class
    std::vector<T>      classes_prob;
    std::vector<size_t> index;

    const auto   output_net  = reinterpret_cast<T *>(tensor.buffer() + tensor.info()->offset_first_element_in_bytes());
    const size_t num_classes = tensor.info()->dimension(0);

    classes_prob.resize(num_classes);
    index.resize(num_classes);

    std::copy(output_net, output_net + num_classes, classes_prob.begin());

    // Sort results
    std::iota(std::begin(index), std::end(index), static_cast<size_t>(0));
    std::sort(std::begin(index), std::end(index),
              [&](size_t a, size_t b)
    {
        return classes_prob[a] > classes_prob[b];
    });

    _output_stream << "---------- Top " << _top_n << " predictions ----------" << std::endl
                   << std::endl;
    for(size_t i = 0; i < _top_n; ++i)
    {
        _output_stream << std::fixed << std::setprecision(4)
                       << +classes_prob[index.at(i)]
                       << " - [id = " << index.at(i) << "]"
                       << ", " << _labels[index.at(i)] << std::endl;
    }
}

ConnectionAccessor::ConnectionAccessor(bool tran){
	//std::cout<<"\nconnectionaccessor1\n";
	transition=tran;
}
ConnectionAccessor2::ConnectionAccessor2(bool tran){
	//std::cout<<"\nconnectionaccessor2\n";
	transition=tran;
}

//Ehsan
//Output of first graph
template <typename T>
void ConnectionAccessor::my_access_predictions_tensor(ITensor &tensor)
{
	// Get the predicted class
    //std::vector<T>      classes_prob;
    //std::vector<size_t> index;
    //const auto   output_net  = reinterpret_cast<T *>(tensor.buffer() + tensor.info()->offset_first_element_in_bytes());
    //const size_t num_classes = tensor.info()->dimension(0);
    //classes_prob.resize(num_classes);
    //index.resize(num_classes);

    //Ehsan
    //std::ostream &t;
    //const size_t num_bytes = tensor.info()->total_size();
    //std::vector<T>  elements;
#if My_print > 0
    std::cout<<"\nGraphUtils,TopNPredictionsAccessor::access_predictions_tensor\n"
    		<<"output tensor shape:"<<tensor.info()->tensor_shape()
			<<" total sizes:"<<tensor.info()->total_size()
			<<std::endl;
			//<<"\n tensor print:\n"<<tensor.print(t);
#endif

#if My_print > 0
    int cnt=0;
    for(size_t offset = 0; offset < tensor.info()->total_size(); offset += tensor.info()->element_size())
    {
         const auto value = *reinterpret_cast<T *>(tensor.buffer() + offset);
         std::cout<<"i:"<<cnt<<" v:"<<value<<"   ";
         if (cnt%8==0)
        	 std::cout<<std::endl;
         cnt++;
    }
#endif


    {
    	//std::cout<<"\n first graph waiting for mutex\n";
		std::lock_guard<std::mutex> lk(m1);
		if(!waiting1){
					//std::cout<<"second graph is busy. first graph want to push on q\n";
					//auto tstart=std::chrono::high_resolution_clock::now();
					////Tensors_Q.push(dynamic_cast<arm_compute::Tensor*>(&(f_out->handle()->tensor())));
					transit_tensor1.copy_from(f_out->handle()->tensor());
				    Tensors_Q1.push(&transit_tensor1);
					//auto tfinish=std::chrono::high_resolution_clock::now();
					//double cost0 = std::chrono::duration_cast<std::chrono::duration<double>>(tfinish - tstart).count();
					//std::cout<<"\npushing to queue time:"<<cost0<<std::endl;
					//std::cout<<"\npushing to queue\n";
					//Tensors_Q.push(*f_out);
		}

		else{

			//std::cout<<"\nDirectly should be transfomed\n";
			//std::cout<<"\n first graph sends directly\n";
			//if(f_out->desc().target==arm_compute::graph::Target ::CL)
			if(transition)
			{
				//std::cout<<"first graph is responsible for transition\n";
				auto tstart=std::chrono::high_resolution_clock::now();
				s_in->handle()->tensor().copy_from(tensor);
				auto tfinish=std::chrono::high_resolution_clock::now();
				double cost0 = std::chrono::duration_cast<std::chrono::duration<double>>(tfinish - tstart).count();
				//std::cout<<"\nTransfer0 time:"<<cost0<<std::endl<<std::endl;
			}
			data_ready1 = true;
			cv1.notify_one();
		}
    }



    /*
    //First_CL (uncomment two lines above for; which map() and one line after for which unmap)
    auto s_handle = s_in->handle();
    s_handle->map(true);
    for(size_t offset = 0; offset < tensor.info()->total_size(); offset += tensor.info()->element_size())
    {
        *reinterpret_cast<T *>(s_in->handle()->tensor().buffer() + offset) = *reinterpret_cast<T *>(tensor.buffer() + offset);
    }
    s_in->handle()->unmap();
    */

    //Or
    //s_in->handle()->tensor().copyfrom(tensor);

    /*
     //asserts should be enabled
    std::cout<<"\nHere\n";
    std::ostream& s = std::cout;
    tensor.print(s);
	*/

    //std::copy(output_net, output_net + num_bytes, elements.begin());
}



//Ehsan
//Output of second graph
template <typename T>
void ConnectionAccessor2::my_access_predictions_tensor(ITensor &tensor)
{
	// Get the predicted class
    //std::vector<T>      classes_prob;
    //std::vector<size_t> index;
    //const auto   output_net  = reinterpret_cast<T *>(tensor.buffer() + tensor.info()->offset_first_element_in_bytes());
    //const size_t num_classes = tensor.info()->dimension(0);
    //classes_prob.resize(num_classes);
    //index.resize(num_classes);

    //Ehsan
    //std::ostream &t;
    //const size_t num_bytes = tensor.info()->total_size();
    //std::vector<T>  elements;
#if My_print > 0
    std::cout<<"\nGraphUtils,TopNPredictionsAccessor::access_predictions_tensor\n"
    		<<"output tensor shape:"<<tensor.info()->tensor_shape()
			<<" total sizes:"<<tensor.info()->total_size()
			<<std::endl;
			//<<"\n tensor print:\n"<<tensor.print(t);
#endif

#if My_print > 0
    int cnt=0;
    for(size_t offset = 0; offset < tensor.info()->total_size(); offset += tensor.info()->element_size())
    {
         const auto value = *reinterpret_cast<T *>(tensor.buffer() + offset);
         std::cout<<"i:"<<cnt<<" v:"<<value<<"   ";
         if (cnt%8==0)
        	 std::cout<<std::endl;
         cnt++;
    }
#endif


    {
    	//std::cout<<"\n second graph sender waiting for mutex\n";
		std::lock_guard<std::mutex> lk(m2);
		if(!waiting2){
					//std::cout<<"third graph is busy. second graph want to push on q\n";
					//auto tstart=std::chrono::high_resolution_clock::now();
					////Tensors_Q.push(dynamic_cast<arm_compute::Tensor*>(&(f_out->handle()->tensor())));
					transit_tensor2.copy_from(s_out->handle()->tensor());
				    Tensors_Q2.push(&transit_tensor2);
					//auto tfinish=std::chrono::high_resolution_clock::now();
					//double cost0 = std::chrono::duration_cast<std::chrono::duration<double>>(tfinish - tstart).count();
					//std::cout<<"\npushing to queue time:"<<cost0<<std::endl;
					//std::cout<<"\npushing to queue\n";
					//Tensors_Q.push(*f_out);
		}

		else{

			//std::cout<<"\nDirectly should be transfomed\n";
		    //if(s_out->desc().target==arm_compute::graph::Target ::CL)
			//std::cout<<"third graph is waiting. second graph want to push directly\n";
			if(transition)
		    {
				//std::cout<<"second graph is responsible for transition\n";
				auto tstart=std::chrono::high_resolution_clock::now();
				t_in->handle()->tensor().copy_from(tensor);
				auto tfinish=std::chrono::high_resolution_clock::now();
				double cost0 = std::chrono::duration_cast<std::chrono::duration<double>>(tfinish - tstart).count();
				//std::cout<<"\nTransfer0 time:"<<cost0<<std::endl<<std::endl;
		    }
			data_ready2 = true;
			cv2.notify_one();
		}
    }



    /*
    //First_CL (uncomment two lines above for; which map() and one line after for which unmap)
    auto s_handle = s_in->handle();
    s_handle->map(true);
    for(size_t offset = 0; offset < tensor.info()->total_size(); offset += tensor.info()->element_size())
    {
        *reinterpret_cast<T *>(s_in->handle()->tensor().buffer() + offset) = *reinterpret_cast<T *>(tensor.buffer() + offset);
    }
    s_in->handle()->unmap();
    */

    //Or
    //s_in->handle()->tensor().copyfrom(tensor);

    /*
     //asserts should be enabled
    std::cout<<"\nHere\n";
    std::ostream& s = std::cout;
    tensor.print(s);
	*/

    //std::copy(output_net, output_net + num_bytes, elements.begin());

}

bool TopNPredictionsAccessor::access_tensor(ITensor &tensor)
{
    ARM_COMPUTE_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(&tensor, 1, DataType::F32, DataType::QASYMM8);
    //Ehsan
    //ARM_COMPUTE_ERROR_ON(_labels.size() != tensor.info()->dimension(0));

    switch(tensor.info()->data_type())
    {
        case DataType::QASYMM8:
            access_predictions_tensor<uint8_t>(tensor);
            break;
        case DataType::F32:
            access_predictions_tensor<float>(tensor);
        	//Ehsan
        	//my_access_predictions_tensor<float>(tensor);
            break;
        default:
            ARM_COMPUTE_ERROR("NOT SUPPORTED!");
    }

    return false;
}

//Ehsan
bool ConnectionAccessor::access_tensor(ITensor &tensor)
{
    ARM_COMPUTE_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(&tensor, 1, DataType::F32, DataType::QASYMM8);
    //Ehsan
    //ARM_COMPUTE_ERROR_ON(_labels.size() != tensor.info()->dimension(0));

    switch(tensor.info()->data_type())
    {
        case DataType::QASYMM8:
            my_access_predictions_tensor<uint8_t>(tensor);
            break;
        case DataType::F32:
            //access_predictions_tensor<float>(tensor);
        	//Ehsan
        	my_access_predictions_tensor<float>(tensor);
            break;
        default:
            ARM_COMPUTE_ERROR("NOT SUPPORTED!");
    }

    return false;
}

//Ehsan
bool ConnectionAccessor2::access_tensor(ITensor &tensor)
{
    ARM_COMPUTE_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(&tensor, 1, DataType::F32, DataType::QASYMM8);
    //Ehsan
    //ARM_COMPUTE_ERROR_ON(_labels.size() != tensor.info()->dimension(0));

    switch(tensor.info()->data_type())
    {
        case DataType::QASYMM8:
            my_access_predictions_tensor<uint8_t>(tensor);
            break;
        case DataType::F32:
            //access_predictions_tensor<float>(tensor);
        	//Ehsan
        	my_access_predictions_tensor<float>(tensor);
            break;
        default:
            ARM_COMPUTE_ERROR("NOT SUPPORTED!");
    }

    return false;
}

RandomAccessor::RandomAccessor(PixelValue lower, PixelValue upper, std::random_device::result_type seed)
    : _lower(lower), _upper(upper), _seed(seed)
{
}

template <typename T, typename D>
void RandomAccessor::fill(ITensor &tensor, D &&distribution)
{
    std::mt19937 gen(_seed);

    if(tensor.info()->padding().empty() && (dynamic_cast<SubTensor *>(&tensor) == nullptr))
    {
        for(size_t offset = 0; offset < tensor.info()->total_size(); offset += tensor.info()->element_size())
        {
            const auto value                                 = static_cast<T>(distribution(gen));
            *reinterpret_cast<T *>(tensor.buffer() + offset) = value;
        }
    }
    else
    {
        // If tensor has padding accessing tensor elements through execution window.
        Window window;
        window.use_tensor_dimensions(tensor.info()->tensor_shape());

        execute_window_loop(window, [&](const Coordinates & id)
        {
            const auto value                                  = static_cast<T>(distribution(gen));
            *reinterpret_cast<T *>(tensor.ptr_to_element(id)) = value;
        });
    }
}

bool RandomAccessor::access_tensor(ITensor &tensor)
{
    switch(tensor.info()->data_type())
    {
        case DataType::QASYMM8:
        case DataType::U8:
        {
            std::uniform_int_distribution<uint8_t> distribution_u8(_lower.get<uint8_t>(), _upper.get<uint8_t>());
            fill<uint8_t>(tensor, distribution_u8);
            break;
        }
        case DataType::S8:
        {
            std::uniform_int_distribution<int8_t> distribution_s8(_lower.get<int8_t>(), _upper.get<int8_t>());
            fill<int8_t>(tensor, distribution_s8);
            break;
        }
        case DataType::U16:
        {
            std::uniform_int_distribution<uint16_t> distribution_u16(_lower.get<uint16_t>(), _upper.get<uint16_t>());
            fill<uint16_t>(tensor, distribution_u16);
            break;
        }
        case DataType::S16:
        {
            std::uniform_int_distribution<int16_t> distribution_s16(_lower.get<int16_t>(), _upper.get<int16_t>());
            fill<int16_t>(tensor, distribution_s16);
            break;
        }
        case DataType::U32:
        {
            std::uniform_int_distribution<uint32_t> distribution_u32(_lower.get<uint32_t>(), _upper.get<uint32_t>());
            fill<uint32_t>(tensor, distribution_u32);
            break;
        }
        case DataType::S32:
        {
            std::uniform_int_distribution<int32_t> distribution_s32(_lower.get<int32_t>(), _upper.get<int32_t>());
            fill<int32_t>(tensor, distribution_s32);
            break;
        }
        case DataType::U64:
        {
            std::uniform_int_distribution<uint64_t> distribution_u64(_lower.get<uint64_t>(), _upper.get<uint64_t>());
            fill<uint64_t>(tensor, distribution_u64);
            break;
        }
        case DataType::S64:
        {
            std::uniform_int_distribution<int64_t> distribution_s64(_lower.get<int64_t>(), _upper.get<int64_t>());
            fill<int64_t>(tensor, distribution_s64);
            break;
        }
        case DataType::F16:
        {
            arm_compute::utils::uniform_real_distribution_16bit<half> distribution_f16(_lower.get<float>(), _upper.get<float>());
            fill<half>(tensor, distribution_f16);
            break;
        }
        case DataType::F32:
        {
            std::uniform_real_distribution<float> distribution_f32(_lower.get<float>(), _upper.get<float>());
            fill<float>(tensor, distribution_f32);
            break;
        }
        case DataType::F64:
        {
            std::uniform_real_distribution<double> distribution_f64(_lower.get<double>(), _upper.get<double>());
            fill<double>(tensor, distribution_f64);
            break;
        }
        default:
            ARM_COMPUTE_ERROR("NOT SUPPORTED!");
    }
    return true;
}

NumPyBinLoader::NumPyBinLoader(std::string filename, DataLayout file_layout)
    : _already_loaded(false), _filename(std::move(filename)), _file_layout(file_layout)
{
}

bool NumPyBinLoader::access_tensor(ITensor &tensor)
{
    if(!_already_loaded)
    {
        utils::NPYLoader loader;
        loader.open(_filename, _file_layout);
        loader.fill_tensor(tensor);
    }

    _already_loaded = !_already_loaded;
    return _already_loaded;
}