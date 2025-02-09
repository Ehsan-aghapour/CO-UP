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
#include<chrono>
#include <sys/types.h>
#include <dirent.h>
//#include "arm_compute/graph/Workload.h"


#include "arm_compute/graph.h"
#include "support/ToolchainSupport.h"
#include "utils/CommonGraphOptions.h"
#include "utils/GraphUtils.h"
#include "utils/Utils.h"

using namespace arm_compute;
using namespace arm_compute::utils;
using namespace arm_compute::graph::frontend;
using namespace arm_compute::graph_utils;


//Ehsan 
typedef std::vector<std::string> stringvec;
void read_directory(const std::string& name, stringvec& v)
{
    DIR* dirp = opendir(name.c_str());
    struct dirent * dp;
    while ((dp = readdir(dirp)) != NULL) {
        if(arm_compute::utility::endswith(dp->d_name, ".ppm"))
           v.push_back(name+(dp->d_name));
    }
    closedir(dirp);
}

//Ehsan
size_t image_index=0;
stringvec images_list;
bool imgs=0;
std::set<int> mobile_blocking {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,27,30};
int end_tasks[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,27,30};
int qend_tasks[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,27,30};
//std::map<std::string,double> task_times;

/** Example demonstrating how to implement MobileNet's network using the Compute Library's graph API */
class GraphMobilenetExample : public Example
{
public:
    GraphMobilenetExample()
        : cmd_parser(), common_opts(cmd_parser), common_params()
    {
        // Add model id option
        model_id_opt = cmd_parser.add_option<SimpleOption<int>>("model-id", 0);
        model_id_opt->set_help("Mobilenet model id (0: 1.0_224, else: 0.75_160");
    }
    GraphMobilenetExample(const GraphMobilenetExample &) = delete;
    GraphMobilenetExample &operator=(const GraphMobilenetExample &) = delete;
    ~GraphMobilenetExample() override                               = default;




    void Attach_Layer(){
    	//std::cerr<<"attaching layer "<<Layer<<" on graph:"<<gr_layer[Layer]<<std::endl;
    	static int start_Layer=0;
    	static int end_Layer=0;
    	Layer++;
    	bool graph_finished=false;
    	if(Layer==Layers)
    		graph_finished=true;
    	//else if(classes[gr_layer[Layer]]!=classes[gr_layer[Layer-1]]){
    	else if(gr_layer[Layer]!=gr_layer[Layer-1]){
    		graph_finished=true;
    	}
    	//std::cerr<<common_params.order[Layer-1]<<", finish: "<<graph_finished<<std::endl;
		if( graph_finished){
			end_Layer=Layer-1;
			if(gr_layer[Layer-1]!=-1){
				if(Layer!=Layers){
					if(targets[gr_layer[Layer-1]]==arm_compute::graph::Target ::CL){
						common_params.labels="transfer";
						//common_params.image="transfer";
					}
					else{
						common_params.labels="transfer_wait";
					}
					if(gr_layer[Layer]==-1)
						common_params.labels="";
					//(*sub_graph)<<OutputLayer(get_Sender_accessor(common_params, gr_layer[Layer-1]+1));
					(*sub_graph)<<OutputLayer(get_Sender_accessor(common_params, Transmitters.size()+1));
				}

				GraphConfig config;
				if(classes[gr_layer[Layer-1]]==0){
					config.num_threads = common_params.threads2;
					//config.cluster=0;
				}
				else{
					config.num_threads = common_params.threads;
					//config.cluster=1;
				}
				config.cluster=classes[gr_layer[Layer-1]];
				config.use_tuner   = common_params.enable_tuner;
				config.tuner_mode  = common_params.tuner_mode;
				config.tuner_file  = common_params.tuner_file;
				config.mlgo_file   = common_params.mlgo_file;
				config.convert_to_uint8 = (common_params.data_type == DataType::QASYMM8);
				if(common_params.data_type == DataType::QASYMM8){
					memcpy(end_tasks,qend_tasks,sizeof(end_tasks));
				}
				//std::cout<<"Finalizing graph_"<<gr_layer[Layer-1]<<"\t after Layer:"<<Layer-1<<std::endl;
				//std::cout<<"class:"<<config.cluster<<"\t target:"<<int(targets[gr_layer[Layer-1]])<<'='<<int(common_params.target)<<std::endl;
				std::set<int> e_t;
				int offset=0;
				if(start_Layer>0)
					offset=end_tasks[start_Layer-1]+1;
				for(int i=start_Layer;i<=end_Layer;i++){
					e_t.insert(end_tasks[i]-offset);
				}
				std::cout<<"Start_Layer:"<<start_Layer<<" \t End layer:"<<end_Layer<<"\n set:";
				for (auto itr = e_t.begin(); itr != e_t.end(); itr++)
				{
					std::cout << *itr<<" ";
				}
				std::cout<<std::endl;
				//sub_graph->finalize(common_params.target, config, &squeeze_blocking,common_params.layer_time);
				sub_graph->finalize(common_params.target, config, &e_t,common_params.layer_time);
				if(gr_layer[Layer-1]>0){
					for(auto &node : sub_graph->graph().nodes())
					{
						if(node != nullptr && node->type() == arm_compute::graph::NodeType::Input)
						{
							//PrintThread{}<<"adding rec "<<Layer<<std::endl;
							if(common_params.image!=""){
								Receivers.push_back(node->output(0));
								continue;
							}
						}
					}
				}
			}

			else if(Layer!=Layers){
				common_params.labels="";
				(*sub_graph)<<OutputLayer(get_Sender_accessor(common_params, gr_layer[Layer]));
			}

			if(Layer!=Layers){
				arm_compute::graph::Tensor* temp_sender;
				TensorShape tshape;
				//if(gr_layer[Layer]!=-1){
					for(auto &node : sub_graph->graph().nodes())
					{
						if(node != nullptr && node->type() == arm_compute::graph::NodeType::Output)
						{
							if(gr_layer[Layer-1]!=-1 && gr_layer[Layer]!=-1){
								Transmitters.push_back(node->input(0));
								//tshape=Transmitters[gr_layer[Layer-1]]->desc().shape;
								tshape=Transmitters[Transmitters.size()-1]->desc().shape;
							}
							else{
								temp_sender=node->input(0);
								tshape=temp_sender->desc().shape;
							}
							continue;
						}
					}

				if(gr_layer[Layer]!=-1){
					sub_graph=(graphs[gr_layer[Layer]]);

					if(gr_layer[Layer-1]==-1){
						common_params.image="";
					}
					else{
						if(classes[gr_layer[Layer-1]]==2){
							common_params.image="transfer_wait";
						}
						else{
							common_params.image="transfer";
						}
					}

					common_params.target=targets[gr_layer[Layer]];
					const auto        operation_layout = common_params.data_layout;
					TensorDescriptor input_descriptor = TensorDescriptor(tshape, common_params.data_type).set_layout(operation_layout);
					(*sub_graph) << common_params.target
								  << common_params.fast_math_hint;
					//std::cout<<common_params.image<<", "<<Transmitters.size()-1<<std::endl;

					//auto tt=InputLayer(input_descriptor, get_Receiver_accessor(common_params,gr_layer[Layer]-1));
					//auto tt=InputLayer(input_descriptor, get_Receiver_accessor(common_params,Transmitters.size()-1));

					(*sub_graph)<<InputLayer(input_descriptor, get_Receiver_accessor(common_params,Transmitters.size()-1));

					cpu_set_t set;
					CPU_ZERO(&set);
					CPU_SET(core[classes[gr_layer[Layer]]],&set);
					ARM_COMPUTE_EXIT_ON_MSG(sched_setaffinity(0, sizeof(set), &set), "Error setting thread affinity");
				}
				else{
					delete dump_graph;
					dump_graph=new Stream(1000,"AlexNet");
					sub_graph=dump_graph;
					common_params.target=arm_compute::graph::Target ::NEON;
					const auto        operation_layout = common_params.data_layout;
					TensorDescriptor input_descriptor = TensorDescriptor(tshape, common_params.data_type).set_layout(operation_layout);
					(*sub_graph) << common_params.target
								  << common_params.fast_math_hint;
					common_params.image="";
					(*sub_graph)<<InputLayer(input_descriptor, get_Receiver_accessor(common_params,gr_layer[Layer]-1));
				}
			}
			start_Layer=Layer;
		}
		//std::cerr<<"Attached\n";
    }



    bool do_setup(int argc, char **argv) override
    {

        // Parse arguments
        cmd_parser.parse(argc, argv);
        cmd_parser.validate();

        // Consume common parameters
        common_params = consume_common_graph_parameters(common_opts);
        //common_params.data_type=DataType::F32;

        //Ehsan
        imgs=!(common_params.image.empty());
        if(imgs){
        	read_directory(common_params.image,images_list);
        	std::cout<<images_list.size()<<" Input images are read from "<<common_params.image<<std::endl;
        	common_params.image=images_list[image_index];
        }

        // Return when help menu is requested
        if(common_params.help)
        {
            cmd_parser.print_help(argv[0]);
            return false;
        }

        // Print parameter values
        //std::cout << common_params << std::endl;

        // Get model parameters
        int model_id = model_id_opt->value();

        // Create input descriptor
        unsigned int spatial_size = (model_id == 0 || common_params.data_type == DataType::QASYMM8) ? 224 : 160;

        // Create input descriptor
        const TensorShape tensor_shape     = permute_shape(TensorShape(spatial_size, spatial_size, 3U, 1U), DataLayout::NCHW, common_params.data_layout);
        TensorDescriptor  input_descriptor = TensorDescriptor(tensor_shape, common_params.data_type).set_layout(common_params.data_layout);




        //Ehsan
        //**********************************************************************************

        int n_l=28;
        std::cerr<<"Number of Layers: "<<n_l<<std::endl;
        std::string lbl=common_params.labels;
        if(common_params.order.size()==1){
        	common_params.order=std::string(n_l, common_params.order[0]);
        }
        if(common_params.order[1]=='-'){
        	common_params.order=std::string(common_params.partition_point,common_params.order[0])+
        			std::string(common_params.partition_point2-common_params.partition_point,common_params.order[2])+
					std::string(n_l-common_params.partition_point2,common_params.order[4]);
        }
        std::string order=common_params.order;

        Layers=order.size();
        int g=0;
        for(int i=0;i<Layers;i++){
        	if(i==0){
        		//Stream graph(i,"AlexNet");
				if (order[i]=='B'){
					targets.push_back(arm_compute::graph::Target ::NEON);
					classes.push_back(1);
				}
				if (order[i]=='L'){
					targets.push_back(arm_compute::graph::Target ::NEON);
					classes.push_back(0);
				}
				if (order[i]=='G'){
					targets.push_back(arm_compute::graph::Target ::CL);
					classes.push_back(2);
				}
				if (order[i]!='-'){
					graphs.push_back(new Stream(g,"AlexNet"));
					gr_layer[i]=g;
				}
				if(order[i]=='-'){
					gr_layer[i]=-1;
				}
        	}

        	else if (order[i]!=order[i-1]){
        		//Stream graph(i,"AlexNet");
        		if(order[i]=='-'){
        			gr_layer[i]=-1;
        		}
        		else{
        			if (order[i]=='B'){
						targets.push_back(arm_compute::graph::Target ::NEON);
						classes.push_back(1);
					}
					if (order[i]=='L'){
						targets.push_back(arm_compute::graph::Target ::NEON);
						classes.push_back(0);
					}
					if (order[i]=='G'){
						targets.push_back(arm_compute::graph::Target ::CL);
						classes.push_back(2);
					}

					graphs.push_back(new Stream(g+1,"AlexNet"));
					gr_layer[i]=graphs.size()-1;
					g=graphs.size()-1;
        		}

        	}

        	else{
        		if(order[i]!='-')
        			gr_layer[i]=g;
        		else
        			gr_layer[i]=-1;
        	}
        }
        for(int i=0;i<Layers;i++){
        	//std::cerr<<i<<"\t"<<gr_layer[i]<<std::endl;
        	if(order[i]=='-'){
        		dump_graph=new Stream(1000,"AlexNEt");
        		break;
        	}
        }
        per_frame=(graphs.size()>1);

        cpu_set_t set;
		CPU_ZERO(&set);
		CPU_SET(core[classes[gr_layer[Layer]]],&set);
		ARM_COMPUTE_EXIT_ON_MSG(sched_setaffinity(0, sizeof(set), &set), "Error setting thread affinity");

        std::cout << common_params << std::endl;

        annotate=common_params.annotate;
		//ann=annotate;
		save_model=common_params.save;

        if(gr_layer[Layer]==-1){
        	sub_graph=dump_graph;
        	common_params.target=arm_compute::graph::Target ::NEON;
        }
        else{
        	sub_graph=(graphs[gr_layer[Layer]]);
        	common_params.target=targets[gr_layer[Layer]];
        }


        //***************************************************************


        // Set graph hints
        (*sub_graph) << common_params.target
              << common_params.fast_math_hint;

        // Create core graph
        if(arm_compute::is_data_type_float(common_params.data_type))
        {
            create_graph_float(input_descriptor, model_id);
        }
        else
        {
            create_graph_qasymm(input_descriptor);
        }

        common_params.labels=lbl;
        // Create common tail
        (*sub_graph) << ReshapeLayer(TensorShape(1001U)).set_name("Reshape")
              << SoftmaxLayer().set_name("Softmax")
              << OutputLayer(get_output_accessor(common_params, 5));

		Attach_Layer();

		im_acc=dynamic_cast<ImageAccessor*>(graphs[0]->graph().node(0)->output(0)->accessor());

		std::cout<<"Total layers:"<<Layer<<std::endl<<std::endl;

		return true;
    }
    void do_run() override
    {
        // Run graph
        //Ehsan
    	std::string t;
    	std::vector<std::thread*> stages;
    	int n=common_params.n;
    	for(int i=0;i<graphs.size();i++){
    		stages.push_back(new std::thread(&GraphMobilenetExample::run,this,i));
    		//std::cout<<"thread "<< i<<" created\n";
    		//stages[i]->join();
    	}
    	for(int i=0;i<stages.size();i++){
			stages[i]->join();
    	}
    	for(int i=0;i<graphs.size();i++){
			//std::cout<<"graph_id: "<<i<<" \t start: "<<graphs[i]->get_start_time().time_since_epoch().count()<<" \t end: "<<graphs[i]->get_finish_time().time_since_epoch().count()<<std::endl;
    		if(common_params.layer_time)
    				graphs[i]->measure(n);

			double tot=graphs[i]->get_input_time()+graphs[i]->get_task_time()+graphs[i]->get_output_time();
			PrintThread{}<<"\n\nCost"<<i<<":"<<1000*graphs[i]->get_cost_time()/n<<std::endl;
			PrintThread{}<<"input"<<i<<"_time:"<<1000*graphs[i]->get_input_time()/n<<"\ntask"<<i<<"_time:"<<1000*graphs[i]->get_task_time()/n<<"\noutput"<<i<<"_time:"<<1000*graphs[i]->get_output_time()/n<<"\ntotal"<<i<<"_time:"<<1000*tot/n<<std::endl;
			std::cout<<"***************************************\n\n";

		}


    	std::cout<<"Frame Latency: "<<1000*latency/(common_params.n)<<std::endl;
    	del();

    }
    void run(int graph_id){
		int cl=classes[graph_id];
		int core_id=core[cl];
		cpu_set_t set;
		CPU_ZERO(&set);
		CPU_SET(core_id,&set);
		ARM_COMPUTE_EXIT_ON_MSG(sched_setaffinity(0, sizeof(set), &set), "Error setting thread affinity");
		PrintThread{}<<"start running graph "<<graph_id<<std::flush<<std::endl;
		double in=0;
		double task=0;
		double out=0;
		int n=(common_params.n);
		bool layer_timing=common_params.layer_time;
		bool end=(graph_id==graphs.size()-1);
		latency=0;
		//auto tstart=std::chrono::high_resolution_clock::now();
		//std::cerr<<"graph__id:"<<graph_id<<"   time:"<<tstart.time_since_epoch().count()<<std::endl;
		if(imgs && graph_id==0){
			if(image_index>=images_list.size())
					image_index=image_index%images_list.size();
			PrintThread{}<<"\n\nFirst graph inferencing image: "<<image_index<<":"<<images_list[image_index]<<std::endl;
			//std::unique_ptr<ImageAccessor> im_acc=dynamic_cast<ImageAccessor*>(graph.graph().node(0)->output(0)->accessor());
			im_acc->set_filename(images_list[image_index++]);
		}
		if(layer_timing){
			//std::cerr<<i<<" graph_id:"<<graph_id<<"   time:"<<std::chrono::high_resolution_clock::now().time_since_epoch().count()<<std::endl;
			graphs[graph_id]->run(annotate,n);
			//graphs[graph_id]->set_finish_time(std::chrono::high_resolution_clock::now());
		}
		else{
			graphs[graph_id]->run(annotate);
		}

		graphs[graph_id]->set_input_time(0);
		graphs[graph_id]->set_task_time(0);
		graphs[graph_id]->set_output_time(0);
		graphs[graph_id]->set_cost_time(0);
		if(layer_timing)
			graphs[graph_id]->reset();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		if(graph_id==0){
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
		auto tstart=std::chrono::high_resolution_clock::now();

		//std::cout<<tstart.time_since_epoch().count()<<std::endl;
		if(graph_id==0)
			start=std::chrono::high_resolution_clock::now();
		for(int i=0;i<n;i++){
			if(imgs && graph_id==0){
				if(image_index>=images_list.size())
						image_index=image_index%images_list.size();
				PrintThread{}<<"\n\nFirst graph inferencing image: "<<image_index<<":"<<images_list[image_index]<<std::endl;
				//std::unique_ptr<ImageAccessor> im_acc=dynamic_cast<ImageAccessor*>(graph.graph().node(0)->output(0)->accessor());
				im_acc->set_filename(images_list[image_index++]);
			}
			if(layer_timing){
				//std::cerr<<i<<" graph_id:"<<graph_id<<"   time:"<<std::chrono::high_resolution_clock::now().time_since_epoch().count()<<std::endl;

				graphs[graph_id]->run(annotate,n);
				//graphs[graph_id]->set_finish_time(std::chrono::high_resolution_clock::now());
				if(end){
					//latency += std::chrono::duration_cast<std::chrono::duration<double>>(graphs[graph_id]->get_finish_time() - graphs[0]->get_start_time()).count();
					latency += std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - start).count();
					start=std::chrono::high_resolution_clock::now();
				}
			}
			else{
				graphs[graph_id]->run(annotate);
			}
		}
		auto tfinish=std::chrono::high_resolution_clock::now();
		double cost0 = std::chrono::duration_cast<std::chrono::duration<double>>(tfinish - tstart).count();
		//graphs[graph_id]->set_input_time(in);
		//graphs[graph_id]->set_task_time(task);
		//graphs[graph_id]->set_output_time(out);
		graphs[graph_id]->set_cost_time(cost0);
		/*double Cost=cost0/n;
		in=in/n;
		task=task/n;
		out=out/n;
		double tot=in+task+out;
		PrintThread{}<<"\n\nCost"<<graph_id<<":"<<Cost<<std::endl;
		PrintThread{}<<"input"<<graph_id<<"_time:"<<in<<"\ntask"<<graph_id<<"_time:"<<task<<"\noutput"<<graph_id<<"_time:"<<out<<"\ntotal"<<graph_id<<"_time:"<<tot<<std::endl;
		std::cout<<"***************************************\n\n";*/


	}



private:
    CommandLineParser  cmd_parser;
    CommonGraphOptions common_opts;
    SimpleOption<int> *model_id_opt{ nullptr };
    CommonGraphParams  common_params;
    std::vector<Stream*> graphs;
    std::vector<arm_compute::graph::Target> targets;
    std::vector<int> classes;
    std::vector<TensorDescriptor> inputs;
    Stream *sub_graph=NULL;
    int Layer=0;
    int Layers=0;
    bool			   annotate{false};
    std::map<int, int> core = {{0, 1}, {1, 5}, {2, 4}};
    ImageAccessor *im_acc=NULL;
    Stream *dump_graph=NULL;
    std::map<int,int> gr_layer;
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    //std::chrono::time_point<std::chrono::high_resolution_clock> finish;
    double latency=0;



    void create_graph_float(TensorDescriptor &input_descriptor, int model_id)
    {
        float       depth_scale = (model_id == 0) ? 1.f : 0.75;
        std::string model_path  = (model_id == 0) ? "/cnn_data/mobilenet_v1_1_224_model/" : "/cnn_data/mobilenet_v1_075_160_model/";

        // Create a preprocessor object
        std::unique_ptr<IPreprocessor> preprocessor = std::make_unique<TFPreproccessor>();

        // Get trainable parameters data path
        std::string data_path = common_params.data_path;

        // Add model path to data path
        if(!data_path.empty())
        {
            data_path += model_path;
        }



        //Ehsan
        Layer=0;

        (*sub_graph) << InputLayer(input_descriptor,
                            get_input_accessor(common_params, std::move(preprocessor), false))
              << ConvolutionLayer(
                  3U, 3U, 32U * depth_scale,
                  get_weights_accessor(data_path, "Conv2d_0_weights.npy", DataLayout::NCHW),
                  std::unique_ptr<arm_compute::graph::ITensorAccessor>(nullptr),
                  PadStrideInfo(2, 2, 0, 1, 0, 1, DimensionRoundingType::FLOOR))
              .set_name("Conv2d_0")
              << BatchNormalizationLayer(
                  get_weights_accessor(data_path, "Conv2d_0_BatchNorm_moving_mean.npy"),
                  get_weights_accessor(data_path, "Conv2d_0_BatchNorm_moving_variance.npy"),
                  get_weights_accessor(data_path, "Conv2d_0_BatchNorm_gamma.npy"),
                  get_weights_accessor(data_path, "Conv2d_0_BatchNorm_beta.npy"),
                  0.001f)
              .set_name("Conv2d_0/BatchNorm")
              << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::BOUNDED_RELU, 6.f)).set_name("Conv2d_0/Relu6");

        Attach_Layer();

        // Layer 2
        get_dwsc_node_float(data_path, "Conv2d_1", 64 * depth_scale, PadStrideInfo(1, 1, 1, 1), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

        // Layer 4
        get_dwsc_node_float(data_path, "Conv2d_2", 128 * depth_scale, PadStrideInfo(2, 2, 0, 1, 0, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

        // Layer 6
        get_dwsc_node_float(data_path, "Conv2d_3", 128 * depth_scale, PadStrideInfo(1, 1, 1, 1, 1, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

        // Layer 8
        get_dwsc_node_float(data_path, "Conv2d_4", 256 * depth_scale, PadStrideInfo(2, 2, 0, 1, 0, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

        // Layer 10
        get_dwsc_node_float(data_path, "Conv2d_5", 256 * depth_scale, PadStrideInfo(1, 1, 1, 1, 1, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

        // Layer 12
        get_dwsc_node_float(data_path, "Conv2d_6", 512 * depth_scale, PadStrideInfo(2, 2, 0, 1, 0, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

        // Layer 14
        get_dwsc_node_float(data_path, "Conv2d_7", 512 * depth_scale, PadStrideInfo(1, 1, 1, 1, 1, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

        // Layer 16
        get_dwsc_node_float(data_path, "Conv2d_8", 512 * depth_scale, PadStrideInfo(1, 1, 1, 1, 1, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

        // Layer 18
        get_dwsc_node_float(data_path, "Conv2d_9", 512 * depth_scale, PadStrideInfo(1, 1, 1, 1, 1, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

        // Layer 20
        get_dwsc_node_float(data_path, "Conv2d_10", 512 * depth_scale, PadStrideInfo(1, 1, 1, 1, 1, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

		// Layer 22
        get_dwsc_node_float(data_path, "Conv2d_11", 512 * depth_scale, PadStrideInfo(1, 1, 1, 1, 1, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

        // Layer 24
        get_dwsc_node_float(data_path, "Conv2d_12", 1024 * depth_scale, PadStrideInfo(2, 2, 0, 1, 0, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));

        Attach_Layer();

        // Layer 26
        get_dwsc_node_float(data_path, "Conv2d_13", 1024 * depth_scale, PadStrideInfo(1, 1, 1, 1, 1, 1, DimensionRoundingType::CEIL), PadStrideInfo(1, 1, 0, 0));
        (*sub_graph) << PoolingLayer(PoolingLayerInfo(PoolingType::AVG, common_params.data_layout)).set_name("Logits/AvgPool_1a");

        Attach_Layer();

        // Layer 28
        (*sub_graph)<< ConvolutionLayer(
                  1U, 1U, 1000U,
                  get_weights_accessor(data_path, "Logits_Conv2d_1c_1x1_weights.npy", DataLayout::NCHW),
                  get_weights_accessor(data_path, "Logits_Conv2d_1c_1x1_biases.npy"),
                  PadStrideInfo(1, 1, 0, 0))
              .set_name("Logits/Conv2d_1c_1x1");
    }















    void create_graph_qasymm(TensorDescriptor &input_descriptor)
    {
        // Get trainable parameters data path
        std::string data_path = common_params.data_path;

        // Add model path to data path
        if(!data_path.empty())
        {
            data_path += "/cnn_data/mobilenet_qasymm8_model/";
        }

        // Quantization info taken from the AndroidNN QASYMM8 MobileNet example
        const QuantizationInfo in_quant_info = QuantizationInfo(0.0078125f, 128);

        const std::vector<QuantizationInfo> conv_weights_quant_info =
        {
            QuantizationInfo(0.02182667888700962f, 151), // conv0
            QuantizationInfo(0.004986600950360298f, 74)  // conv14
        };
        const std::vector<QuantizationInfo> conv_out_quant_info =
        {
            QuantizationInfo(0.023528477177023888f, 0), // conv0
            QuantizationInfo(0.16609922051429749f, 66)  // conv14
        };

        const std::vector<QuantizationInfo> depth_weights_quant_info =
        {
            QuantizationInfo(0.29219913482666016f, 110),  // dwsc1
            QuantizationInfo(0.40277284383773804f, 130),  // dwsc2
            QuantizationInfo(0.06053730100393295f, 160),  // dwsc3
            QuantizationInfo(0.01675807684659958f, 123),  // dwsc4
            QuantizationInfo(0.04105526953935623f, 129),  // dwsc5
            QuantizationInfo(0.013460792601108551f, 122), // dwsc6
            QuantizationInfo(0.036934755742549896f, 132), // dwsc7
            QuantizationInfo(0.042609862983226776f, 94),  // dwsc8
            QuantizationInfo(0.028358859941363335f, 127), // dwsc9
            QuantizationInfo(0.024329448118805885f, 134), // dwsc10
            QuantizationInfo(0.019366811960935593f, 106), // dwsc11
            QuantizationInfo(0.007835594937205315f, 126), // dwsc12
            QuantizationInfo(0.12616927921772003f, 211)   // dwsc13
        };

        const std::vector<QuantizationInfo> point_weights_quant_info =
        {
            QuantizationInfo(0.030420949682593346f, 121), // dwsc1
            QuantizationInfo(0.015148180536925793f, 104), // dwsc2
            QuantizationInfo(0.013755458407104015f, 94),  // dwsc3
            QuantizationInfo(0.007601846940815449f, 151), // dwsc4
            QuantizationInfo(0.006431614048779011f, 122), // dwsc5
            QuantizationInfo(0.00917122047394514f, 109),  // dwsc6
            QuantizationInfo(0.005300046876072884f, 140), // dwsc7
            QuantizationInfo(0.0049632852897048f, 127),   // dwsc8
            QuantizationInfo(0.007770895957946777f, 89),  // dwsc9
            QuantizationInfo(0.009658650495111942f, 99),  // dwsc10
            QuantizationInfo(0.005446993745863438f, 153), // dwsc11
            QuantizationInfo(0.00817922968417406f, 130),  // dwsc12
            QuantizationInfo(0.018048152327537537f, 95)   // dwsc13
        };
        //Ehsan
        Layer=0;

        (*sub_graph) << InputLayer(input_descriptor.set_quantization_info(in_quant_info),
                            get_input_accessor(common_params, nullptr, false))
              << ConvolutionLayer(
                  3U, 3U, 32U,
                  get_weights_accessor(data_path, "Conv2d_0_weights.npy"),
                  get_weights_accessor(data_path, "Conv2d_0_bias.npy"),
                  PadStrideInfo(2U, 2U, 0U, 1U, 0U, 1U, DimensionRoundingType::FLOOR),
                  1, conv_weights_quant_info.at(0), conv_out_quant_info.at(0))
              .set_name("Conv2d_0")
              << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::LU_BOUNDED_RELU, 6.f)).set_name("Conv2d_0/Relu6");

        Attach_Layer();

        // Layer 2
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_1", 64U, PadStrideInfo(1U, 1U, 1U, 1U), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(0), point_weights_quant_info.at(0));

        Attach_Layer();

        // Layer 4
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_2", 128U, PadStrideInfo(2U, 2U, 0U, 1U, 0U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(1),
                                      point_weights_quant_info.at(1));

        Attach_Layer();

        // Layer 6
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_3", 128U, PadStrideInfo(1U, 1U, 1U, 1U, 1U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(2),
                                      point_weights_quant_info.at(2));

        Attach_Layer();

        // Layer 8
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_4", 256U, PadStrideInfo(2U, 2U, 0U, 1U, 0U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(3),
                                      point_weights_quant_info.at(3));

        Attach_Layer();

        // Layer 10
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_5", 256U, PadStrideInfo(1U, 1U, 1U, 1U, 1U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(4),
                                      point_weights_quant_info.at(4));
        Attach_Layer();

        // Layer 12
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_6", 512U, PadStrideInfo(2U, 2U, 0U, 1U, 0U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(5),
                                      point_weights_quant_info.at(5));

        Attach_Layer();

        // Layer 14
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_7", 512U, PadStrideInfo(1U, 1U, 1U, 1U, 1U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(6),
                                      point_weights_quant_info.at(6));

        Attach_Layer();

        // Layer 16
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_8", 512U, PadStrideInfo(1U, 1U, 1U, 1U, 1U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(7),
                                      point_weights_quant_info.at(7));

        Attach_Layer();

        // Layer 18
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_9", 512U, PadStrideInfo(1U, 1U, 1U, 1U, 1U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(8),
                                      point_weights_quant_info.at(8));

        Attach_Layer();

        // Layer 20
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_10", 512U, PadStrideInfo(1U, 1U, 1U, 1U, 1U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(9),
                                      point_weights_quant_info.at(9));

        Attach_Layer();

        // Layer 22
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_11", 512U, PadStrideInfo(1U, 1U, 1U, 1U, 1U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(10),
                                      point_weights_quant_info.at(10));

        Attach_Layer();

        // Layer 24
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_12", 1024U, PadStrideInfo(2U, 2U, 0U, 1U, 0U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(11),
                                      point_weights_quant_info.at(11));

        Attach_Layer();

        // Layer 26
        (*sub_graph) << get_dwsc_node_qasymm(data_path, "Conv2d_13", 1024U, PadStrideInfo(1U, 1U, 1U, 1U, 1U, 1U, DimensionRoundingType::FLOOR), PadStrideInfo(1U, 1U, 0U, 0U), depth_weights_quant_info.at(12),
                                      point_weights_quant_info.at(12))
              << PoolingLayer(PoolingLayerInfo(PoolingType::AVG, common_params.data_layout)).set_name("Logits/AvgPool_1a");

        Attach_Layer();

        // Layer 28
             (*sub_graph) << ConvolutionLayer(
                  1U, 1U, 1001U,
                  get_weights_accessor(data_path, "Logits_Conv2d_1c_1x1_weights.npy"),
                  get_weights_accessor(data_path, "Logits_Conv2d_1c_1x1_bias.npy"),
                  PadStrideInfo(1U, 1U, 0U, 0U), 1, conv_weights_quant_info.at(1), conv_out_quant_info.at(1))
              .set_name("Logits/Conv2d_1c_1x1");
    }


    void get_dwsc_node_float(const std::string &data_path, std::string &&param_path,
                                    unsigned int  conv_filt,
                                    PadStrideInfo dwc_pad_stride_info, PadStrideInfo conv_pad_stride_info)
    {
        std::string total_path = param_path + "_";
        //SubStream   sg(*sub_graph);
        (*sub_graph) << DepthwiseConvolutionLayer(
               3U, 3U,
               get_weights_accessor(data_path, total_path + "depthwise_depthwise_weights.npy", DataLayout::NCHW),
               std::unique_ptr<arm_compute::graph::ITensorAccessor>(nullptr),
               dwc_pad_stride_info)
           .set_name(total_path + "depthwise/depthwise")
           << BatchNormalizationLayer(
               get_weights_accessor(data_path, total_path + "depthwise_BatchNorm_moving_mean.npy"),
               get_weights_accessor(data_path, total_path + "depthwise_BatchNorm_moving_variance.npy"),
               get_weights_accessor(data_path, total_path + "depthwise_BatchNorm_gamma.npy"),
               get_weights_accessor(data_path, total_path + "depthwise_BatchNorm_beta.npy"),
               0.001f)
           .set_name(total_path + "depthwise/BatchNorm")
           << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::BOUNDED_RELU, 6.f)).set_name(total_path + "depthwise/Relu6");

        Attach_Layer();

        (*sub_graph) << ConvolutionLayer(
               1U, 1U, conv_filt,
               get_weights_accessor(data_path, total_path + "pointwise_weights.npy", DataLayout::NCHW),
               std::unique_ptr<arm_compute::graph::ITensorAccessor>(nullptr),
               conv_pad_stride_info)
           .set_name(total_path + "pointwise/Conv2D")
           << BatchNormalizationLayer(
               get_weights_accessor(data_path, total_path + "pointwise_BatchNorm_moving_mean.npy"),
               get_weights_accessor(data_path, total_path + "pointwise_BatchNorm_moving_variance.npy"),
               get_weights_accessor(data_path, total_path + "pointwise_BatchNorm_gamma.npy"),
               get_weights_accessor(data_path, total_path + "pointwise_BatchNorm_beta.npy"),
               0.001f)
           .set_name(total_path + "pointwise/BatchNorm")
           << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::BOUNDED_RELU, 6.f)).set_name(total_path + "pointwise/Relu6");

        //return ConcatLayer(std::move(sg));
    }




    ConcatLayer get_dwsc_node_qasymm(const std::string &data_path, std::string &&param_path,
                                     const unsigned int conv_filt,
                                     PadStrideInfo dwc_pad_stride_info, PadStrideInfo conv_pad_stride_info,
                                     QuantizationInfo depth_weights_quant_info, QuantizationInfo point_weights_quant_info)
    {
        std::string total_path = param_path + "_";
        SubStream   sg(*sub_graph);

        sg << DepthwiseConvolutionLayer(
               3U, 3U,
               get_weights_accessor(data_path, total_path + "depthwise_weights.npy"),
               get_weights_accessor(data_path, total_path + "depthwise_bias.npy"),
               dwc_pad_stride_info, 1, std::move(depth_weights_quant_info))
           .set_name(total_path + "depthwise/depthwise")
           << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::LU_BOUNDED_RELU, 6.f)).set_name(total_path + "depthwise/Relu6");

        Attach_Layer();

        sg   << ConvolutionLayer(
               1U, 1U, conv_filt,
               get_weights_accessor(data_path, total_path + "pointwise_weights.npy"),
               get_weights_accessor(data_path, total_path + "pointwise_bias.npy"),
               conv_pad_stride_info, 1, std::move(point_weights_quant_info))
           .set_name(total_path + "pointwise/Conv2D")
           << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::LU_BOUNDED_RELU, 6.f)).set_name(total_path + "pointwise/Relu6");

        return ConcatLayer(std::move(sg));
    }
};

/** Main program for MobileNetV1
 *
 * Model is based on:
 *      https://arxiv.org/abs/1704.04861
 *      "MobileNets: Efficient Convolutional Neural Networks for Mobile Vision Applications"
 *      Andrew G. Howard, Menglong Zhu, Bo Chen, Dmitry Kalenichenko, Weijun Wang, Tobias Weyand, Marco Andreetto, Hartwig Adam
 *
 * Provenance: download.tensorflow.org/models/mobilenet_v1_2018_08_02/mobilenet_v1_1.0_224.tgz
 *             download.tensorflow.org/models/mobilenet_v1_2018_08_02/mobilenet_v1_0.75_160.tgz
 *
 * @note To list all the possible arguments execute the binary appended with the --help option
 *
 * @param[in] argc Number of arguments
 * @param[in] argv Arguments
 */
int main(int argc, char **argv)
{
    return arm_compute::utils::run_example<GraphMobilenetExample>(argc, argv);
}
