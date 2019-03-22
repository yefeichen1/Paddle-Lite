///* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.
//
//Licensed under the Apache License, Version 2.0 (the "License");
//you may not use this file except in compliance with the License.
//You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
//Unless required by applicable law or agreed to in writing, software
//distributed under the License is distributed on an "AS IS" BASIS,
//WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//See the License for the specific language governing permissions and
//limitations under the License. */
//
//#include "framework/executor_multikernel_impl.h"
//#include <algorithm>
//#include <utility>
//#include <vector>
//#include "common/enforce.h"
//#include "common/log.h"
//#include "framework/context.h"
//#include "framework/framework.pb-c.h"
//#include "framework/lod_tensor.h"
//#include "framework/operator.h"
//#include "framework/program/program-optimize/program_optimize.h"
//#include "framework/program/program_desc.h"
//#include "framework/program/var_desc.h"
//#include "framework/scope.h"
//#include "framework/tensor.h"
//#include "memory/t_malloc.h"
//
//#ifdef PADDLE_MOBILE_CL
//#include "framework/cl/cl_image.h"
//#endif
//
//namespace paddle_mobile {
//namespace framework {
//
//#pragma mark - executor
//
//template <typename T>
//void ExecutorMultiKernel<T>::SetThreadNum(int threads) {
//  set_global_num_threads(threads);
//}
//
//template <typename T>
//ExecutorMultiKernel<T>::ExecutorMultiKernel(
//    const Program<T> &program, paddle_mobile::PaddleMobileConfigInternal config,
//    int batch_size, const bool use_optimize, const bool lod_mode)
//    : program_(program),
//      batch_size_(batch_size),
//      use_optimize_(use_optimize),
//      lod_mode_(lod_mode),
//      config_(config) {
//  DLOG << "executor in lod mode: " << lod_mode_;
//
//  Variable *variable_ptr = program_.scope->Var("batch_size");
//  variable_ptr->SetValue<int>(batch_size);
//
//  program_desc_ =
//      use_optimize_ ? program_.optimizeProgram : program_.originProgram;
//  PADDLE_MOBILE_ENFORCE(program_desc_ != nullptr,
//                        "program_desc_ should not be nullptr");
//  // resize feed and fetch list
//  // should init feed and fetch variables before infer shape
//  InitFeedFetchList();
//
//  const auto &blocks = program_desc_->Blocks();
//  std::shared_ptr<BlockDesc> block_desc = blocks[0];
//
//  //
//  std::vector<std::shared_ptr<OpDesc>> ops = block_desc->Ops();
//  const vector<std::shared_ptr<VarDesc>> &vars = block_desc->Vars();
//
//  for (int j = 0; j < ops.size(); ++j) {
//    std::shared_ptr<OpDesc> op_desc = ops[j];
//    if (OpInfoMap<GPU_CL>::Instance()->Has(op_desc->Type())) {
//      DLOG << "create op : " << op_desc->Type() << "  use gpu";
//
//      auto op_handler_gpu = OpRegistry<GPU_CL>::CreateOp(
//          op_desc->Type(), op_desc->GetInputs(), op_desc->GetOutputs(),
//          op_desc->GetAttrMap(), program_.scope.get());
//
//      // infer shape to reshape inputs and outputs before predict,
//      // but for lod mode, it still need to infer shape in runtime
//      if (!lod_mode) {
//        op_handler_gpu->InferShape();
//      }
//      op_desc->SetComputeType(ComputeType::ComputeGPU);
//      ops_of_block0_.push_back(&op_handler_gpu.get());
//
//    } else if (OpInfoMap<CPU>::Instance()->Has(op_desc->Type())) {
//      DLOG << "create op : " << op_desc->Type() << "  use cpu";
//
//      auto op_handler_cpu = OpRegistry<CPU>::CreateOp(
//          op_desc->Type(), op_desc->GetInputs(), op_desc->GetOutputs(),
//          op_desc->GetAttrMap(), program_.scope.get());
//
//      // infer shape to reshape inputs and outputs before predict,
//      // but for lod mode, it still need to infer shape in runtime
//      if (!lod_mode) {
//        op_handler_cpu->InferShape();
//      }
//      op_desc->SetComputeType(ComputeType::ComputeCPU);
//
//      ops_of_block0_.push_back(&(op_handler_cpu.get()));
//
//    } else {
//      PADDLE_MOBILE_ENFORCE(false, "unsupported op ( %s ) want to create",
//                            op_desc->Type().c_str());
//    }
//
//    // 拿到某个打印op相关信息.
//
//    AttributeMap &opAttrMap = op_desc->GetAttrMap();
//
//    for (AttributeMap::iterator iter = opAttrMap.begin();
//         iter != opAttrMap.end(); iter++) {
//      DLOG << "attr:  " << iter->first << "\t" << iter->second;
//    }
//
//    // var presient --- > use type of input and output
//
//    // var in out  judge from last and next op
//
//    VariableNameMap &inputs = op_desc->GetInputs();
//    for (VariableNameMap::iterator input_iter = inputs.begin();
//         input_iter != inputs.end(); input_iter++) {
//      auto ins = input_iter->second;
//      DLOG << "inputs:  " << input_iter->first << "\t" << ins;
//
//      for (auto in : ins) {
//        auto find_var = std::find(vars.begin(), vars.end(), in);
//        // 找到,并且是实例化在本地 给这个op 使用的.
//        if (find_var != vars.end() && find_var->get()->Persistable()) {
//          find_var->get()->SetComputeType(op_desc->GetComputeType());
//        }
//
//        /* for (auto var : vars) {
//           if (var->Name() == in) {
//             var.get()->SetComputeType(op_desc->GetComputeType());
//           }
//         }*/
//      }
//    }
//
//    VariableNameMap &outputs = op_desc->GetOutputs();
//    for (VariableNameMap::iterator output_iter = outputs.begin();
//         output_iter != outputs.end(); output_iter++) {
//      auto outs = output_iter->second;
//      DLOG << "outputs:  " << output_iter->first << "\t" << outs;
//
//      for (auto out : outs) {
//        auto find_var = std::find(vars.begin(), vars.end(), out);
//        // 找到,并且是实例化在本地 给这个op 使用的.
//        if (find_var != vars.end() && find_var->get()->Persistable()) {
//          find_var->get()->SetComputeType(op_desc->GetComputeType());
//        }
//
//        /*
//          for (auto var : vars) {
//            if (var->Name() == out) {
//              var.get()->SetComputeType(op_desc->GetComputeType());
//            }
//          }*/
//      }
//    }
//
//    //    AttributeMap &opAttrMap = op_desc->GetAttrMap();
//    //
//    //    for (AttributeMap::iterator iter = opAttrMap.begin();
//    //         iter != opAttrMap.end(); iter++) {
//    //      DLOG << iter->first<< "\t" << iter->second;
//    //    }
//  }
//
//  if (program_.combined) {
//    InitCombineMemory();
//  } else {
//    InitMemory();
//  }
//
//  int count = 0;
//  for (auto &op_handler : ops_of_block0_) {
//    DLOG << "Initialize op[" << count++ << "]: " << op_handler->Type();
//    op_handler->Init();
//  }
//
//  for (auto &op_handler : ops_of_block0_) {
//    DLOG << "Initialize op[" << count++ << "]: " << op_handler->Type();
//    op_handler->Init();
//  }
//}
//
//template <typename T>
//void ExecutorMultiKernel<T>::InitFeedFetchList() {
//  std::unordered_map<std::string, int> feed_indices, fetch_indices;
//  for (const auto &block : program_desc_->Blocks()) {
//    for (const auto &op_desc : block->Ops()) {
//      if (op_desc->Type() == "feed") {
//        std::string name = op_desc->Output("Out")[0];
//        feed_indices[name] = op_desc->GetAttr("col").Get<int>();
//      } else if (op_desc->Type() == "fetch") {
//        std::string name = op_desc->Input("X")[0];
//        fetch_indices[name] = op_desc->GetAttr("col").Get<int>();
//      }
//    }
//  }
//  feed_indices_.swap(feed_indices);
//  fetch_indices_.swap(fetch_indices);
//
//  auto *feed_var = program_.scope->Var("feed");
//  auto *feed_list = feed_var->template GetMutable<framework::LoDTensorArray>();
//  feed_list->resize(feed_indices_.size());
//
//  auto *fetch_var = program_.scope->Var("fetch");
//  auto *fetch_list =
//      fetch_var->template GetMutable<framework::LoDTensorArray>();
//  fetch_list->resize(fetch_indices_.size());
//}
//
//template <typename T>
//static void LoadMemInternal(void **data, LoDTensor *tensor,
//                            bool quant_uint8 = false) {
//  char **data_buf = reinterpret_cast<char **>(data);
//  int64_t size = tensor->numel();
//  T *tensor_data = tensor->mutable_data<T>();
//  if (quant_uint8) {
//    // should be moved into operator init function
//    float min_value;
//    float max_value;
//    memory::Copy(&min_value, *data_buf, sizeof(float));
//    memory::Copy(&max_value, *data_buf + sizeof(float), sizeof(float));
//    *data_buf += 2 * sizeof(float);
//    const float factor = (max_value - min_value) / 255.0;
//    const uint8_t *uint8_data = reinterpret_cast<uint8_t *>(*data_buf);
//    for (int k = 0; k < size; ++k) {
//      tensor_data[k] = uint8_data[k] * factor + min_value;
//    }
//    *data_buf += size * sizeof(uint8_t);
//  } else {
//    memory::Copy(tensor_data, *data_buf, size * sizeof(T));
//    *data_buf += size * sizeof(T);
//  }
//}
//
//template <typename T>
//void ExecutorMultiKernel<T>::LoadMemory(void **data,
//                                        const std::shared_ptr<VarDesc> var_desc,
//                                        LoDTensor *tensor) {
//  char **data_buf = reinterpret_cast<char **>(data);
//  // version
//  uint32_t version = *(reinterpret_cast<uint32_t *>(*data_buf));
//  *data_buf += sizeof(uint32_t);
//  // lod information
//  // uint64_t lod_level = *(reinterpret_cast<uint64_t *>(*data_buf));
//  uint64_t lod_level = 0;
//  memory::Copy(&lod_level, *data_buf, sizeof(uint64_t));
//  *data_buf += sizeof(uint64_t);
//
//  auto *lod = tensor->mutable_lod();
//  lod->resize(lod_level);
//  for (uint64_t i = 0; i < lod_level; ++i) {
//    uint64_t size = *(reinterpret_cast<uint64_t *>(*data_buf));
//    *data_buf += sizeof(uint64_t);
//    std::vector<size_t> tmp_dim(size / sizeof(size_t));
//    memory::Copy(tmp_dim.data(), *data_buf, size);
//    (*lod)[i] = std::move(tmp_dim);
//    *data_buf += size;
//  }
//  // tensor version
//  uint32_t tensor_version = *(reinterpret_cast<uint32_t *>(*data_buf));
//  *data_buf += sizeof(uint32_t);
//  // tensor desc size
//  int32_t tensor_desc_size = *(reinterpret_cast<int32_t *>(*data_buf));
//  *data_buf += sizeof(int32_t);
//  // skip tensor desc
//  *data_buf += tensor_desc_size;
//
//  const TensorDesc &tensor_desc = var_desc->Tensor_desc();
//  tensor->Resize(make_ddim(tensor_desc.Dims()));
//  // parse tensor from stream
//  switch (tensor_desc.DataType()) {
//    case VARTYPE_TYPE_FP32:
//      LoadMemInternal<float>(reinterpret_cast<void **>(data_buf), tensor,
//                             program_.quantification);
//      break;
//    case VARTYPE_TYPE_INT8:
//      LoadMemInternal<int8_t>(reinterpret_cast<void **>(data_buf), tensor);
//      break;
//    case VARTYPE_TYPE_INT32:
//      LoadMemInternal<int>(reinterpret_cast<void **>(data_buf), tensor);
//      break;
//    default:
//      LOG(kLOG_ERROR) << "data type is not supported";
//  }
//}
//
//template <typename T>
//void ExecutorMultiKernel<T>::InitMemory() {
//  for (const auto &block : program_desc_->Blocks()) {
//    for (const auto &var_desc : block->Vars()) {
//      auto var = program_.scope->Var(var_desc->Name());
//      if (var_desc->Persistable()) {
//        if (var_desc->Name() == "feed" || var_desc->Name() == "fetch") {
//          var->template GetMutable<framework::LoDTensorArray>();
//          continue;
//        }
//        char *origin_data =
//            ReadFileToBuff(program_.model_path + "/" + var_desc->Name());
//        char *data = origin_data;
//        auto tensor = var->template GetMutable<LoDTensor>();
//        LoadMemory(reinterpret_cast<void **>(&data), var_desc, tensor);
//        delete[] origin_data;
//      } else {
//        DLOG << "init no persistable var: " << var_desc->Name();
//        varInputMemory(var_desc, var);
//      }
//    }
//  }
//}
//
//template <typename T>
//void ExecutorMultiKernel<T>::InitCombineMemory() {
//  char *origin_data = nullptr;
//  bool self_alloc = false;
//  if (program_.combined_params_buf && program_.combined_params_len) {
//    origin_data = reinterpret_cast<char *>(program_.combined_params_buf);
//  } else {
//    self_alloc = true;
//    origin_data = ReadFileToBuff(program_.para_path);
//  }
//  PADDLE_MOBILE_ENFORCE(origin_data != nullptr, "data == nullptr");
//  char *data = origin_data;
//  for (const auto &block : program_desc_->Blocks()) {
//    for (const auto &var_desc : block->Vars()) {
//      auto var = program_.scope->Var(var_desc->Name());
//      if (var_desc->Persistable()) {
//        if (var_desc->Name() == "feed" || var_desc->Name() == "fetch") {
//          var->template GetMutable<framework::LoDTensorArray>();
//          continue;
//        }
//
//        DLOG << " init combine memory persistable: " << var_desc->Name();
//        auto tensor = var->template GetMutable<LoDTensor>();
//        LoadMemory(reinterpret_cast<void **>(&data), var_desc, tensor);
//      } else {
//        DLOG << " init combine memory no persistable: " << var_desc->Name();
//        varInputMemory(var_desc, var);
//      }
//    }
//  }
//  if (self_alloc) {
//    delete[] origin_data;
//  }
//  LOG(kLOG_INFO) << "init combine memory finish";
//}
//
//template <typename T>
//void ExecutorMultiKernel<T>::InitNoPersistableMemory(
//    const Tensor &input_tensor) {
//  for (const auto &block : program_desc_->Blocks()) {
//    for (const auto &var_desc : block->Vars()) {
//      auto var = program_.scope->Var(var_desc->Name());
//      auto tensor = var->template GetMutable<LoDTensor>();
//      if (var_desc->Persistable()) {
//        if (var_desc->Name() == "feed" || var_desc->Name() == "fetch") {
//          var->template GetMutable<framework::LoDTensorArray>();
//          continue;
//        }
//      } else {
//        if (var_desc->Type() == VARTYPE_TYPE_LOD_TENSOR) {
//          DDim tensor_dim = tensor->dims();
//          DDim new_dim =
//              make_ddim({tensor_dim[0], tensor_dim[1], input_tensor.dims()[2],
//                         input_tensor.dims()[3]});
//          tensor->Resize(new_dim);
//          tensor->template mutable_data<T>();
//        } else {
//          PADDLE_MOBILE_THROW_EXCEPTION("Unsupported var type `%d`",
//                                        var_desc->Type());
//        }
//      }
//    }
//  }
//
//  std::shared_ptr<LoDTensor> output = GetOutput("fetch");
//  output->Resize(input_tensor.dims());
//  output->mutable_data<T>();
//}
//
//template <typename T>
//bool ExecutorMultiKernel<T>::varInputMemory(
//    const std::shared_ptr<VarDesc> &var_desc, Variable *var) const {
//  auto TypeId = [](const VarType_Type &type) -> std::type_index {
//    switch (type) {
//      case VARTYPE_TYPE_BOOL:
//        return typeid(bool);
//      case VARTYPE_TYPE_FP32:
//        return typeid(float);
//      case VARTYPE_TYPE_INT8:
//        return typeid(int8_t);
//      case VARTYPE_TYPE_INT32:
//        return typeid(int);
//      case VARTYPE_TYPE_INT64:
//        return typeid(int64_t);
//      default:
//        PADDLE_MOBILE_THROW_EXCEPTION("got unhandled var type `%d`", type);
//    }
//  };
//
//  auto type = var_desc->Type();
//  if (type == VARTYPE_TYPE_LOD_TENSOR) {
//    auto data_type = var_desc->Tensor_desc().DataType();
//    framework::LoDTensor *tensor = var->template GetMutable<LoDTensor>();
//    tensor->mutable_data(TypeId(data_type));
//  } else if (type == VARTYPE_TYPE_STEP_SCOPES) {
//    std::vector<framework::Scope *> *step_scopes =
//        var->template GetMutable<std::vector<framework::Scope *>>();
//  } else if (type == VARTYPE_TYPE_STEP_LOD_TENSOR_ARRAY) {
//    framework::LoDTensorArray *tensor_array =
//        var->template GetMutable<framework::LoDTensorArray>();
//  } else {
//    PADDLE_MOBILE_THROW_EXCEPTION("got unhandled var type `%d`", type);
//  }
//  return true;
//}
//
//template <typename T>
//PMStatus ExecutorMultiKernel<T>::Predict(
//    const std::vector<std::pair<std::string, Tensor>> &inputs) {
//  for (const auto &input : inputs) {
//    SetInput(input.second, input.first);
//  }
//  return this->Predict();
//}
//
//template <typename T>
//PMStatus ExecutorMultiKernel<T>::Predict(
//    const std::vector<std::pair<std::string, LoDTensor>> &inputs) {
//  for (const auto &input : inputs) {
//    SetInput(input.second, input.first);
//  }
//  return this->Predict();
//}
//
//template <typename T>
//std::vector<T> ExecutorMultiKernel<T>::Predict(
//    const std::vector<T> &input, const std::vector<int64_t> &dims) {
//  PADDLE_MOBILE_ENFORCE(feed_indices_.size() != 0,
//                        "We don't know which tensor should be assign, since no "
//                        "feed op found in this model");
//  PADDLE_MOBILE_ENFORCE(fetch_indices_.size() != 0,
//                        "We don't know which tensor should be fetch out, since "
//                        "no fetch op found in this model");
//  std::string input_name = feed_indices_.begin()->first;
//  Tensor feed_tensor(input, make_ddim(dims));
//  SetInput(feed_tensor, input_name);
//  std::vector<T> output;
//  if (this->Predict() == PMSuccess) {
//    std::string output_name = fetch_indices_.begin()->first;
//    const auto output_tensor = GetOutput(output_name);
//    output.resize(output_tensor->numel());
//    memcpy(output.data(), output_tensor->template data<T>(),
//           output.size() * sizeof(T));
//  }
//  return output;
//}
//
//template <typename T>
//void ExecutorMultiKernel<T>::SetInput(const Tensor &input,
//                                      const std::string &var_name) {
//  int index = 0;
//  if (feed_indices_.find(var_name) != feed_indices_.end()) {
//    index = feed_indices_.find(var_name)->second;
//  }
//  auto *feed_var = program_.scope->Var("feed");
//  framework::LoDTensor &target =
//      feed_var->template GetMutable<framework::LoDTensorArray>()->at(index);
//
//  if (config_.load_when_predict) {
//    if (input_dim_last_ != input.dims()) {
//      InitNoPersistableMemory(input);
//      input_dim_last_ = input.dims();
//    }
//  }
//
//  target.Resize(input.dims());
//  target.ShareDataWith(input);
//}
//
//template <typename T>
//void ExecutorMultiKernel<T>::SetInput(const LoDTensor &input,
//                                      const std::string &var_name) {
//  int index = 0;
//  if (feed_indices_.find(var_name) != feed_indices_.end()) {
//    index = feed_indices_.find(var_name)->second;
//  }
//  auto *feed_var = program_.scope->Var("feed");
//  framework::LoDTensor &target =
//      feed_var->template GetMutable<framework::LoDTensorArray>()->at(index);
//
//  if (config_.load_when_predict) {
//    if (input_dim_last_ != input.dims()) {
//      InitNoPersistableMemory(input);
//      input_dim_last_ = input.dims();
//    }
//  }
//
//  target.Resize(input.dims());
//  target.ShareDataWith(input);
//  target.set_lod(input.lod());
//}
//
//template <typename T>
//std::shared_ptr<LoDTensor> ExecutorMultiKernel<T>::GetOutput(
//    const std::string &var_name) {
//  const auto &iter = fetch_indices_.find(var_name);
//  if (var_name == "fetch" || iter != fetch_indices_.end()) {
//    int index = 0;
//    if (iter != fetch_indices_.end()) {
//      index = iter->second;
//    }
//    auto *fetch_var = program_.scope->Var("fetch");
//    framework::LoDTensor &target =
//        fetch_var->template GetMutable<framework::LoDTensorArray>()->at(index);
//
//    return std::make_shared<LoDTensor>(target);
//  } else {
//    auto *fetch_var = program_.scope->Var(var_name);
//    framework::LoDTensor *target =
//        fetch_var->template GetMutable<framework::LoDTensor>();
//    return std::make_shared<LoDTensor>(*target);
//  }
//}
//
//template <typename T>
//PMStatus ExecutorMultiKernel<T>::Predict() {
//#if _OPENMP
//  omp_set_num_threads(get_global_num_threads());
//#endif
//#ifdef PADDLE_MOBILE_PROFILE
//  std::vector<ProfInfo> profile(ops_of_block0_.size());
//  struct timespec ts;
//  int op_index = 0;
//#endif
//  for (auto &op_handler : ops_of_block0_) {
//#ifdef PADDLE_MOBILE_PROFILE
//    clock_gettime(CLOCK_MONOTONIC, &ts);
//    profile[op_index].runBegin = (uint64_t)ts.tv_sec * 1e9 + ts.tv_nsec;
//#endif
//    if (lod_mode_) {
//      op_handler->InferShape();
//    }
//    op_handler->Run();
//#ifdef PADDLE_MOBILE_PROFILE
//    clock_gettime(CLOCK_MONOTONIC, &ts);
//    profile[op_index].runEnd = (uint64_t)ts.tv_sec * 1e9 + ts.tv_nsec;
//    ++op_index;
//#endif
//  }
//#ifdef PADDLE_MOBILE_PROFILE
//  std::unordered_map<std::string, uint64_t> _tp;
//  for (int i = 0; i < profile.size(); i++) {
//    const auto &pInfo = profile[i];
//    uint64_t timeCost = pInfo.runEnd - pInfo.runBegin;
//    if (ops_of_block0_[i]->Type() == "conv2d" ||
//        ops_of_block0_[i]->Type() == "depthwise_conv2d") {
//      auto inputs = ops_of_block0_[i]->Inputs();
//      auto *filter =
//          GetVarValue<LoDTensor>("Filter", inputs, *(program_.scope));
//      int kernel_size = filter->dims()[2];
//      _tp[ops_of_block0_[i]->Type() + "_" + std::to_string(kernel_size)] +=
//          timeCost;
//    } else {
//      _tp[ops_of_block0_[i]->Type()] += timeCost;
//    }
//  }
//  printf("====================[ profile ]======================\n");
//  typedef std::pair<std::string, uint64_t> prof_t;
//  std::vector<prof_t> _tv(_tp.begin(), _tp.end());
//  uint64_t _ptotal = 0;
//  for (auto const &p : _tv) {
//    _ptotal += p.second;
//  }
//  auto compf = [](const prof_t &a, const prof_t &b) {
//    return a.second > b.second;
//  };
//  std::sort(_tv.begin(), _tv.end(), compf);
//  _tv.push_back(std::make_pair("total", _ptotal));
//  for (auto const &p : _tv) {
//    printf("%-16s\t%-10.0f\t%-2.4f\n", p.first.c_str(),
//           static_cast<float>(p.second),
//           static_cast<float>(p.second) / _ptotal * 100.0);
//  }
//  printf("====================[---------]======================\n");
//#endif
//  return PMSuccess;
//}
//
//#ifdef PADDLE_MOBILE_CL
//template <>
//void ExecutorMultiKernel<GPU_CL, float>::InitNoPersistableMemory(
//    const Tensor &input_tensor) {
//  DLOG << "CL InitNoPersistableMemory ";
//  for (const auto &block : program_desc_->Blocks()) {
//    for (const auto &var_desc : block->Vars()) {
//      auto var = program_.scope->Var(var_desc->Name());
//
//      auto cl_image = var->template GetMutable<CLImage>();
//
//      if (var_desc->Persistable()) {
//        if (var_desc->Name() == "feed" || var_desc->Name() == "fetch") {
//          continue;
//        }
//      } else {
//        if (var_desc->Type() == VARTYPE_TYPE_LOD_TENSOR) {
//          cl_context context = program_.scope->GetCLScpoe()->Context();
//          cl_command_queue command_queue =
//              program_.scope->GetCLScpoe()->CommandQueue();
//
//          DDim tensor_dim = cl_image->dims();
//          DDim new_dim =
//              make_ddim({tensor_dim[0], tensor_dim[1], input_tensor.dims()[2],
//                         input_tensor.dims()[3]});
//          cl_image->Resize(new_dim);
//          cl_image->InitEmptyImage(context, command_queue, new_dim);
//        }
//      }
//    }
//  }
//  std::shared_ptr<LoDTensor> output = GetOutput("fetch");
//  output->Resize(input_tensor.dims());
//  output->mutable_data<float>();
//}
//
//template <>
//void ExecutorMultiKernel<GPU_CL, float>::SetInput(const Tensor &input,
//                                                  const std::string &var_name) {
//  int index = 0;
//  if (feed_indices_.find(var_name) != feed_indices_.end()) {
//    index = feed_indices_.find(var_name)->second;
//  }
//  auto *feed_var = program_.scope->Var("feed");
//  framework::LoDTensor *target_tensor =
//      &(feed_var->template GetMutable<framework::LoDTensorArray>()->at(index));
//
//  DLOG << "config_.load_when_predict   " << config_.load_when_predict;
//  DLOG << "target_tensor->IsInitialized() " << target_tensor->IsInitialized();
//  DLOG << "target_tensor->dims()   " << target_tensor->dims();
//  DLOG << "input.dims()   " << input.dims();
//  DLOG << "input_dim_last_   " << input_dim_last_;
//  if (config_.load_when_predict) {
//    if (input_dim_last_ != input.dims()) {
//      DLOG << "SetInput ---- > resize1";
//      target_tensor->Resize(input.dims());
//      target_tensor->mutable_data<float>();
//      InitNoPersistableMemory(*target_tensor);
//    }
//  } else {
//    DLOG << "SetInput ---- > resize2";
//    target_tensor->Resize(input.dims());
//    DLOG << "SetInput ---- > ShareDataWith";
//  }
//  target_tensor->ShareDataWith(input);
//  auto &dim = input.dims();
//  input_dim_last_ = static_cast<DDim>(dim);
//}
//
//template <typename T>
//void ExecutorMultiKernel<T>::LoadMemory(const VarDesc var_desc,
//                                        float *tensorInput, char **data) {}
//
//template <>
//void ExecutorMultiKernel<GPU_CL, float>::LoadMemory(const VarDesc var_desc,
//                                                    float *tensorInput,
//                                                    char **data) {
//  // 1. version
//  uint32_t version = *reinterpret_cast<uint32_t *>(*data);
//
//  (*data) += sizeof(uint32_t);
//
//  // 2 Lod information
//  uint64_t *lod_level_ptr = new uint64_t();
//  memcpy(lod_level_ptr, (*data), sizeof(uint64_t));
//  uint64_t lod_level = *lod_level_ptr;
//  delete lod_level_ptr;
//  (*data) += sizeof(uint64_t);
//
//  for (uint64_t i = 0; i < lod_level; ++i) {
//    uint64_t size = *reinterpret_cast<uint64_t *>(*data);
//    (*data) += sizeof(uint64_t);
//    std::vector<size_t> tmp(size / sizeof(size_t));
//
//    for (int k = 0; k < tmp.size(); ++k) {
//      tmp[k] = *reinterpret_cast<size_t *>(*data);
//      (*data) += sizeof(size_t);
//    }
//  }
//
//  // 3. tensor version
//  uint32_t tensor_version = *reinterpret_cast<uint32_t *>(*data);
//  (*data) += sizeof(uint32_t);
//
//  // 4. tensor desc
//  int32_t size = *reinterpret_cast<int32_t *>(*data);
//  (*data) += sizeof(int32_t);
//
//  std::unique_ptr<char[]> buf(new char[size]);
//  for (int m = 0; m < size; ++m) {
//    buf.get()[m] = (*data)[m];
//  }
//  (*data) += (sizeof(char) * size);
//
//  const TensorDesc &desc = var_desc.Tensor_desc();
//  int memory_size = 1;
//  for (auto l : desc.Dims()) {
//    memory_size *= l;
//  }
//
//  void *memory = nullptr;
//  int type_size = 4;
//  memory = tensorInput;
//  if (program_.quantification) {
//    float min_value;
//    float max_value;
//
//    memcpy(&min_value, *data, sizeof(float));
//    memcpy(&max_value, *data + sizeof(float), sizeof(float));
//    *data += 2 * sizeof(float);
//    const float factor = (max_value - min_value) / 255.0;
//    uint8_t *uint8_data = reinterpret_cast<uint8_t *>(*data);
//    for (int k = 0; k < memory_size; ++k) {
//      static_cast<float *>(memory)[k] = uint8_data[k] * factor + min_value;
//    }
//    *data += (memory_size * sizeof(uint8_t));
//  } else {
//    for (int n = 0; n < memory_size; n++) {
//      float value;
//      memcpy(&value, *data + n * type_size, type_size);
//      if (value < 1e-30 && value > -1e-30) {
//        static_cast<float *>(memory)[n] = 0.0;
//      } else {
//        static_cast<float *>(memory)[n] = value;
//      }
//    }
//    (*data) += (sizeof(char) * memory_size * type_size);
//  }
//}
//
//template <>
//void ExecutorMultiKernel<GPU_CL, float>::InitMemory() {
//  for (const auto &block : program_desc_->Blocks()) {
//    for (const auto &var_desc : block->Vars()) {
//      auto var = program_.scope->Var(var_desc->Name());
//      if (var_desc->Persistable()) {
//        CLImage *cl_image = nullptr;
//        if (var_desc->Name() == "feed" || var_desc->Name() == "fetch") {
//          var->template GetMutable<framework::LoDTensorArray>();
//          continue;
//        } else {
//          cl_image = var->template GetMutable<CLImage>();
//        }
//
//        char *origin_data =
//            ReadFileToBuff(program_.model_path + "/" + var_desc->Name());
//        char *data = origin_data;
//        cl_context context = program_.scope->GetCLScpoe()->Context();
//        const TensorDesc &desc = var_desc->Tensor_desc();
//        int numel = 1;
//        for (auto l : desc.Dims()) {
//          numel *= l;
//        }
//        DLOG << var_desc->Name();
//        float *tensorInput = static_cast<float *>(
//            paddle_mobile::memory::Alloc(sizeof(float) * numel));
//        LoadMemory(*var_desc, tensorInput, &data);
//
//        DDim ddim = make_ddim(desc.Dims());
//
//        // has not init
//        cl_image->SetTensorData(tensorInput, ddim);
//
//        delete origin_data;
//        paddle_mobile::memory::Free(tensorInput);
//      } else {
//        if (var_desc->Type() == VARTYPE_TYPE_LOD_TENSOR) {
//          auto cl_image = var->template GetMutable<CLImage>();
//          cl_context context = program_.scope->GetCLScpoe()->Context();
//          cl_command_queue command_queue =
//              program_.scope->GetCLScpoe()->CommandQueue();
//
//          const TensorDesc &desc = var_desc->Tensor_desc();
//          //          DDim ddim = make_ddim(desc.Dims());
//          DDim ddim = cl_image->dims();
//          DLOG << var_desc->Name();
//          cl_image->InitEmptyImage(context, command_queue, ddim);
//        }
//      }
//    }
//  }
//}
//
//template <>
//void ExecutorMultiKernel<GPU_CL, float>::InitCombineMemory() {
//  DLOG << "CL InitCombineMemory---- "
//       << "config_.load_when_predict: " << config_.load_when_predict;
//  char *origin_data = nullptr;
//  bool self_alloc = false;
//  if (program_.combined_params_buf && program_.combined_params_len) {
//    LOG(kLOG_INFO) << "use outter memory";
//    origin_data = reinterpret_cast<char *>(program_.combined_params_buf);
//  } else {
//    LOG(kLOG_INFO) << " begin init combine memory";
//    self_alloc = true;
//    origin_data = ReadFileToBuff(program_.para_path);
//  }
//  PADDLE_MOBILE_ENFORCE(origin_data != nullptr, "origin_data==nullptr!!!");
//  float *data = reinterpret_cast<float *>(origin_data);
//
//  for (const auto &block : program_desc_->Blocks()) {
//    for (const auto &var_desc : block->Vars()) {
//      auto var = program_.scope->Var(var_desc->Name());
//      if (var_desc->Persistable()) {
//        CLImage *cl_image = nullptr;
//        if (var_desc->Name() == "feed" || var_desc->Name() == "fetch") {
//          var->template GetMutable<framework::LoDTensorArray>();
//          continue;
//        } else {
//          cl_image = var->template GetMutable<CLImage>();
//        }
//
//        cl_context context = program_.scope->GetCLScpoe()->Context();
//
//        const TensorDesc &desc = var_desc->Tensor_desc();
//        DDim ddim = make_ddim(desc.Dims());
//
//        int numel = 1;
//        for (int i = 0; i < ddim.size(); i++) {
//          numel = numel * ddim[i];
//        }
//        float *tensorInput = static_cast<float *>(
//            paddle_mobile::memory::Alloc(sizeof(float) * numel));
//        LoadMemory(*var_desc, tensorInput, &origin_data);
//
//        // has not init
//        cl_image->SetTensorData(tensorInput, ddim);
//
//        paddle_mobile::memory::Free(tensorInput);
//      } else {
//        auto cl_image = var->template GetMutable<CLImage>();
//        cl_context context = program_.scope->GetCLScpoe()->Context();
//        cl_command_queue command_queue =
//            program_.scope->GetCLScpoe()->CommandQueue();
//        const TensorDesc &desc = var_desc->Tensor_desc();
//        DDim ddim = cl_image->dims();
//        //  DDim ddim = make_ddim(desc.Dims());
//        cl_image->InitEmptyImage(context, command_queue, ddim);
//      }
//    }
//  }
//  if (self_alloc) {
//    delete data;
//  }
//  LOG(kLOG_INFO) << " end init combine memory ";
//}
//
//#endif
//
//template class ExecutorMultiKernel<float>;
//
//// template class ExecutorMultiKernel<FPGA, float>;
////
//// template class ExecutorMultiKernel<GPU_CL, float>;
//
//}  // namespace framework
//}  // namespace paddle_mobile