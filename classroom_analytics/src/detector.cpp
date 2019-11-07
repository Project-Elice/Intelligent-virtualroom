// Copyright (C) 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "detector.hpp"

#include <algorithm>
#include <string>
#include <map>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <inference_engine.hpp>
#include <gflags/gflags.h>
#include <functional>
#include <iostream>
#include <fstream>
#include <random>
#include <memory>
#include <chrono>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>
#include <iterator>
#include <map>

#include <inference_engine.hpp>

#include <samples/ocv_common.hpp>
#include <samples/slog.hpp>

#include <ie_iextension.h>
#include <ext_list.hpp>


using namespace InferenceEngine;

#define SSD_EMPTY_DETECTIONS_INDICATOR -1.0

using namespace detection;

namespace {
cv::Rect TruncateToValidRect(const cv::Rect& rect,
                             const cv::Size& size) {
    auto tl = rect.tl(), br = rect.br();
    tl.x = std::max(0, std::min(size.width - 1, tl.x));
    tl.y = std::max(0, std::min(size.height - 1, tl.y));
    br.x = std::max(0, std::min(size.width, br.x));
    br.y = std::max(0, std::min(size.height, br.y));
    int w = std::max(0, br.x - tl.x);
    int h = std::max(0, br.y - tl.y);
    return cv::Rect(tl.x, tl.y, w, h);
}

cv::Rect IncreaseRect(const cv::Rect& r, float coeff_x,
                      float coeff_y)  {
    cv::Point2f tl = r.tl();
    cv::Point2f br = r.br();
    cv::Point2f c = (tl * 0.5f) + (br * 0.5f);
    cv::Point2f diff = c - tl;
    cv::Point2f new_diff{diff.x * coeff_x, diff.y * coeff_y};
    cv::Point2f new_tl = c - new_diff;
    cv::Point2f new_br = c + new_diff;

    cv::Point new_tl_int {static_cast<int>(std::floor(new_tl.x)), static_cast<int>(std::floor(new_tl.y))};
    cv::Point new_br_int {static_cast<int>(std::ceil(new_br.x)), static_cast<int>(std::ceil(new_br.y))};

    return cv::Rect(new_tl_int, new_br_int);
}
}  // namespace

void FaceDetection::submitRequest() {
    if (!enqueued_frames_) return;
    enqueued_frames_ = 0;
    results_fetched_ = false;
    results.clear();
    BaseCnnDetection::submitRequest();
}

void FaceDetection::enqueue(const cv::Mat &frame) {
    if (!enabled()) return;

    if (!request) {
        request = net_.CreateInferRequestPtr();
    }

    width_ = frame.cols;
    height_ = frame.rows;

    Blob::Ptr inputBlob = request->GetBlob(input_name_);

    matU8ToBlob<uint8_t>(frame, inputBlob);

    enqueued_frames_ = 1;
}

FaceDetection::FaceDetection(const DetectorConfig& config) :
    BaseCnnDetection(config.enabled, config.is_async), config_(config) {
    if (config.enabled) {
        topoName = "face detector";
        CNNNetReader net_reader;
        net_reader.ReadNetwork(config.path_to_model);
        net_reader.ReadWeights(config.path_to_weights);
        if (!net_reader.isParseSuccess()) {
            THROW_IE_EXCEPTION << "Cannot load model";
        }

        InputsDataMap inputInfo(net_reader.getNetwork().getInputsInfo());
        if (inputInfo.size() != 1) {
            THROW_IE_EXCEPTION << "Face Detection network should have only one input";
        }
        InputInfo::Ptr inputInfoFirst = inputInfo.begin()->second;
        inputInfoFirst->setPrecision(Precision::U8);
        inputInfoFirst->getInputData()->setLayout(Layout::NCHW);

        SizeVector input_dims = inputInfoFirst->getInputData()->getTensorDesc().getDims();
        input_dims[2] = config_.input_h;
        input_dims[3] = config_.input_w;
        std::map<std::string, SizeVector> input_shapes;
        input_shapes[inputInfo.begin()->first] = input_dims;
        net_reader.getNetwork().reshape(input_shapes);

        OutputsDataMap outputInfo(net_reader.getNetwork().getOutputsInfo());
        if (outputInfo.size() != 1) {
            THROW_IE_EXCEPTION << "Face Detection network should have only one output";
        }
        DataPtr& _output = outputInfo.begin()->second;
        output_name_ = outputInfo.begin()->first;

        const CNNLayerPtr outputLayer = net_reader.getNetwork().getLayerByName(output_name_.c_str());
        if (outputLayer->type != "DetectionOutput") {
            THROW_IE_EXCEPTION << "Face Detection network output layer(" + outputLayer->name +
                                  ") should be DetectionOutput, but was " +  outputLayer->type;
        }

        if (outputLayer->params.find("num_classes") == outputLayer->params.end()) {
            THROW_IE_EXCEPTION << "Face Detection network output layer (" +
                                  output_name_ + ") should have num_classes integer attribute";
        }

        const SizeVector outputDims = _output->getTensorDesc().getDims();
        max_detections_count_ = outputDims[2];
        object_size_ = outputDims[3];
        if (object_size_ != 7) {
            THROW_IE_EXCEPTION << "Face Detection network output layer should have 7 as a last dimension";
        }
        if (outputDims.size() != 4) {
            THROW_IE_EXCEPTION << "Face Detection network output dimensions not compatible shoulld be 4, but was " +
                                  std::to_string(outputDims.size());
        }
        _output->setPrecision(Precision::FP32);
        _output->setLayout(TensorDesc::getLayoutByDims(_output->getDims()));

        input_name_ = inputInfo.begin()->first;
        net_ = config_.plugin.LoadNetwork(net_reader.getNetwork(), config_.device,{});
    }
}

void FaceDetection::fetchResults() {
    if (!enabled()) return;
    results.clear();
    if (results_fetched_) return;
    results_fetched_ = true;
    const float *data = request->GetBlob(output_name_)->buffer().as<float *>();

    for (int det_id = 0; det_id < max_detections_count_; ++det_id) {
        const int start_pos = det_id * object_size_;

        const float batchID = data[start_pos];
        if (batchID == SSD_EMPTY_DETECTIONS_INDICATOR) {
            break;
        }

        const float score = std::min(std::max(0.0f, data[start_pos + 2]), 1.0f);
        const float x0 =
                std::min(std::max(0.0f, data[start_pos + 3]), 1.0f) * width_;
        const float y0 =
                std::min(std::max(0.0f, data[start_pos + 4]), 1.0f) * height_;
        const float x1 =
                std::min(std::max(0.0f, data[start_pos + 5]), 1.0f) * width_;
        const float y1 =
                std::min(std::max(0.0f, data[start_pos + 6]), 1.0f) * height_;

        DetectedObject object;
        object.confidence = score;
        object.rect = cv::Rect(cv::Point(round(x0), round(y0)),
                               cv::Point(round(x1), round(y1)));

        object.rect = TruncateToValidRect(IncreaseRect(object.rect,
                                                       config_.increase_scale_x,
                                                       config_.increase_scale_y),
                                          cv::Size(width_, height_));

        if (object.confidence > config_.confidence_threshold && object.rect.area() > 0) {
            results.emplace_back(object);
        }
    }
}


BaseDetection::BaseDetection(std::string topoName,
                             const std::string &pathToModel,
                             const std::string &deviceForInference,
                             int maxBatch, bool isBatchDynamic, bool isAsync,
                             bool doRawOutputMessages)
    : plugin(nullptr), topoName(topoName), pathToModel(pathToModel), deviceForInference(deviceForInference),
      maxBatch(maxBatch), isBatchDynamic(isBatchDynamic), isAsync(isAsync),
      enablingChecked(false), _enabled(false), doRawOutputMessages(doRawOutputMessages) {
    if (isAsync) {
        slog::info << "Use async mode for " << topoName << slog::endl;
    }
}

BaseDetection::~BaseDetection() {}

ExecutableNetwork* BaseDetection::operator ->() {
    return &net;
}

void BaseDetection::submitRequest() {
    if (!enabled() || request == nullptr) return;
    if (isAsync) {
        request->StartAsync();
    } else {
        request->Infer();
    }
}

void BaseDetection::wait() {
    if (!enabled()|| !request || !isAsync)
        return;
    request->Wait(IInferRequest::WaitMode::RESULT_READY);
}

bool BaseDetection::enabled() const  {
    if (!enablingChecked) {
        _enabled = !pathToModel.empty();
        if (!_enabled) {
            slog::info << topoName << " DISABLED" << slog::endl;
        }
        enablingChecked = true;
    }
    return _enabled;
}

HeadPoseDetection::HeadPoseDetection(const std::string &pathToModel,
                                     const std::string &deviceForInference,
                                     int maxBatch, bool isBatchDynamic, bool isAsync, bool doRawOutputMessages)
    : BaseDetection("Head Pose", pathToModel, deviceForInference, maxBatch, isBatchDynamic, isAsync, doRawOutputMessages),
      outputAngleR("angle_r_fc"), outputAngleP("angle_p_fc"), outputAngleY("angle_y_fc"), enquedFaces(0) {
}

void HeadPoseDetection::submitRequest()  {
    if (!enquedFaces) return;
    if (isBatchDynamic) {
        request->SetBatch(enquedFaces);
    }
    BaseDetection::submitRequest();
    enquedFaces = 0;
}

void HeadPoseDetection::enqueue(const cv::Mat &face) {
    if (!enabled()) {
        return;
    }
    if (enquedFaces == maxBatch) {
        slog::warn << "Number of detected faces more than maximum(" << maxBatch << ") processed by Head Pose estimator" << slog::endl;
        return;
    }
    if (!request) {
        request = net.CreateInferRequestPtr();
    }

    Blob::Ptr inputBlob = request->GetBlob(input);

    matU8ToBlob<uint8_t>(face, inputBlob, enquedFaces);

    enquedFaces++;
}

HeadPoseDetection::Results HeadPoseDetection::operator[] (int idx) const {
    Blob::Ptr  angleR = request->GetBlob(outputAngleR);
    Blob::Ptr  angleP = request->GetBlob(outputAngleP);
    Blob::Ptr  angleY = request->GetBlob(outputAngleY);

    HeadPoseDetection::Results r = {angleR->buffer().as<float*>()[idx],
                                    angleP->buffer().as<float*>()[idx],
                                    angleY->buffer().as<float*>()[idx]};

    if (doRawOutputMessages) {
        std::cout << "[" << idx << "] element, yaw = " << r.angle_y <<
                     ", pitch = " << r.angle_p <<
                     ", roll = " << r.angle_r << std::endl;
    }

    return r;
}

CNNNetwork HeadPoseDetection::read() {
    slog::info << "Loading network files for Head Pose Estimation network" << slog::endl;
    CNNNetReader netReader;
    // Read network model
    netReader.ReadNetwork(pathToModel);
    // Set maximum batch size
    netReader.getNetwork().setBatchSize(maxBatch);
    slog::info << "Batch size is set to  " << netReader.getNetwork().getBatchSize() << " for Head Pose Estimation network" << slog::endl;
    // Extract model name and load its weights
    std::string binFileName = fileNameNoExt(pathToModel) + ".bin";
    netReader.ReadWeights(binFileName);

    // ---------------------------Check inputs -------------------------------------------------------------
    slog::info << "Checking Head Pose Estimation network inputs" << slog::endl;
    InputsDataMap inputInfo(netReader.getNetwork().getInputsInfo());
    if (inputInfo.size() != 1) {
        throw std::logic_error("Head Pose Estimation network should have only one input");
    }
    InputInfo::Ptr& inputInfoFirst = inputInfo.begin()->second;
    inputInfoFirst->setPrecision(Precision::U8);
    input = inputInfo.begin()->first;
    // -----------------------------------------------------------------------------------------------------
    // ---------------------------Check outputs ------------------------------------------------------------
    slog::info << "Checking Head Pose Estimation network outputs" << slog::endl;
    OutputsDataMap outputInfo(netReader.getNetwork().getOutputsInfo());
    if (outputInfo.size() != 3) {
        throw std::logic_error("Head Pose Estimation network should have 3 outputs");
    }
    for (auto& output : outputInfo) {
        output.second->setPrecision(Precision::FP32);
    }
    std::map<std::string, bool> layerNames = {
        {outputAngleR, false},
        {outputAngleP, false},
        {outputAngleY, false}
    };

    for (auto && output : outputInfo) {
        CNNLayerPtr layer = output.second->getCreatorLayer().lock();
        if (!layer) {
            throw std::logic_error("Layer pointer is invalid");
        }
        if (layerNames.find(layer->name) == layerNames.end()) {
            throw std::logic_error("Head Pose Estimation network output layer unknown: " + layer->name + ", should be " +
                                   outputAngleR + " or " + outputAngleP + " or " + outputAngleY);
        }
        if (layer->type != "FullyConnected") {
            throw std::logic_error("Head Pose Estimation network output layer (" + layer->name + ") has invalid type: " +
                                   layer->type + ", should be FullyConnected");
        }
        auto fc = dynamic_cast<FullyConnectedLayer*>(layer.get());
        if (!fc) {
            throw std::logic_error("Fully connected layer is not valid");
        }
        if (fc->_out_num != 1) {
            throw std::logic_error("Head Pose Estimation network output layer (" + layer->name + ") has invalid out-size=" +
                                   std::to_string(fc->_out_num) + ", should be 1");
        }
        layerNames[layer->name] = true;
    }

    slog::info << "Loading Head Pose Estimation model to the "<< deviceForInference << " plugin" << slog::endl;

    _enabled = true;
    return netReader.getNetwork();
}

EmotionsDetection::EmotionsDetection(const std::string &pathToModel,
                                     const std::string &deviceForInference,
                                     int maxBatch, bool isBatchDynamic, bool isAsync, bool doRawOutputMessages)
              : BaseDetection("Emotions Recognition", pathToModel, deviceForInference, maxBatch, isBatchDynamic, isAsync, doRawOutputMessages),
                enquedFaces(0) {
}

void EmotionsDetection::submitRequest() {
    if (!enquedFaces) return;
    if (isBatchDynamic) {
        request->SetBatch(enquedFaces);
    }
    BaseDetection::submitRequest();
    enquedFaces = 0;
}

void EmotionsDetection::enqueue(const cv::Mat &face) {
    if (!enabled()) {
        return;
    }
    if (enquedFaces == maxBatch) {
        slog::warn << "Number of detected faces more than maximum(" << maxBatch << ") processed by Emotions Recognition network" << slog::endl;
        return;
    }
    if (!request) {
        request = net.CreateInferRequestPtr();
    }

    Blob::Ptr inputBlob = request->GetBlob(input);

    matU8ToBlob<uint8_t>(face, inputBlob, enquedFaces);

    enquedFaces++;
}

std::map<std::string, float> EmotionsDetection::operator[] (int idx) const {
    // Vector of supported emotions
    static const std::vector<std::string> emotionsVec = {"neutral", "happy", "sad", "surprise", "anger"};
    auto emotionsVecSize = emotionsVec.size();

    Blob::Ptr emotionsBlob = request->GetBlob(outputEmotions);

    /* emotions vector must have the same size as number of channels
     * in model output. Default output format is NCHW, so index 1 is checked */
    size_t numOfChannels = emotionsBlob->getTensorDesc().getDims().at(1);
    if (numOfChannels != emotionsVec.size()) {
        throw std::logic_error("Output size (" + std::to_string(numOfChannels) +
                               ") of the Emotions Recognition network is not equal "
                              "to used emotions vector size (" +
                               std::to_string(emotionsVec.size()) + ")");
    }

    auto emotionsValues = emotionsBlob->buffer().as<float *>();
    auto outputIdxPos = emotionsValues + idx * emotionsVecSize;
    std::map<std::string, float> emotions;

    if (doRawOutputMessages) {
        std::cout << "[" << idx << "] element, predicted emotions (name = prob):" << std::endl;
    }

    for (size_t i = 0; i < emotionsVecSize; i++) {
        emotions[emotionsVec[i]] = outputIdxPos[i];

        if (doRawOutputMessages) {
            std::cout << emotionsVec[i] << " = " << outputIdxPos[i];
            if (emotionsVecSize - 1 != i) {
                std::cout << ", ";
            } else {
                std::cout << std::endl;
            }
        }
    }

    return emotions;
}
CNNNetwork EmotionsDetection::read() {
    slog::info << "Loading network files for Emotions Recognition" << slog::endl;
    InferenceEngine::CNNNetReader netReader;
    // Read network model
    netReader.ReadNetwork(pathToModel);

    // Set maximum batch size
    netReader.getNetwork().setBatchSize(maxBatch);
    slog::info << "Batch size is set to " << netReader.getNetwork().getBatchSize() << " for Emotions Recognition" << slog::endl;


    // Extract model name and load its weights
    std::string binFileName = fileNameNoExt(pathToModel) + ".bin";
    netReader.ReadWeights(binFileName);

    // -----------------------------------------------------------------------------------------------------

    // Emotions Recognition network should have one input and one output.
    // ---------------------------Check inputs -------------------------------------------------------------
    slog::info << "Checking Emotions Recognition network inputs" << slog::endl;
    InferenceEngine::InputsDataMap inputInfo(netReader.getNetwork().getInputsInfo());
    if (inputInfo.size() != 1) {
        throw std::logic_error("Emotions Recognition network should have only one input");
    }
    auto& inputInfoFirst = inputInfo.begin()->second;
    inputInfoFirst->setPrecision(Precision::U8);
    input = inputInfo.begin()->first;
    // -----------------------------------------------------------------------------------------------------

    // ---------------------------Check outputs ------------------------------------------------------------
    slog::info << "Checking Emotions Recognition network outputs" << slog::endl;
    InferenceEngine::OutputsDataMap outputInfo(netReader.getNetwork().getOutputsInfo());
    if (outputInfo.size() != 1) {
        throw std::logic_error("Emotions Recognition network should have one output layer");
    }
    for (auto& output : outputInfo) {
        output.second->setPrecision(Precision::FP32);
    }

    DataPtr emotionsOutput = outputInfo.begin()->second;

    if (!emotionsOutput) {
        throw std::logic_error("Emotions output data pointer is invalid");
    }

    auto emotionsCreatorLayer = emotionsOutput->getCreatorLayer().lock();

    if (!emotionsCreatorLayer) {
        throw std::logic_error("Emotions creator layer pointer is invalid");
    }

    if (emotionsCreatorLayer->type != "SoftMax") {
        throw std::logic_error("In Emotions Recognition network, Emotion layer ("
                               + emotionsCreatorLayer->name +
                               ") should be a SoftMax, but was: " +
                               emotionsCreatorLayer->type);
    }
    slog::info << "Emotions layer: " << emotionsCreatorLayer->name<< slog::endl;

    outputEmotions = emotionsOutput->getName();

    slog::info << "Loading Emotions Recognition model to the "<< deviceForInference << " plugin" << slog::endl;
    _enabled = true;
    return netReader.getNetwork();
}

Load::Load(BaseDetection& detector) : detector(detector) {
}

void Load::into(Core & plg, std::string device, bool enable_dynamic_batch) const {
    if (detector.enabled()) {
        std::map<std::string, std::string> config;

	    detector.net = plg.LoadNetwork(detector.read(), device, config);
	if(static_cast<IExecutableNetwork::Ptr> (detector.net).get() == nullptr)
                slog::info << "Null pointer detected" << slog::endl;
        detector.plugin = &plg;
    }
}
