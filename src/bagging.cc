/**
 * Copyright (c) 2018 by Marek Wydmuch, Robert Istvan Busa-Fekete
 * All rights reserved.
 */

#include "bagging.h"

#include <random>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <vector>

#include "model.h"

namespace fasttext {

Bagging::Bagging(std::shared_ptr<Args> args) : LossLayer(args){ }

Bagging::~Bagging(){ }

void Bagging::setup(std::shared_ptr<Args> args, std::shared_ptr<Dictionary> dict){
    std::cerr << "Setting up Bagging layer ...\n";
    args_ = args;
    sizeSum = 0;
    args_->randomTree = true;

    assert(args_->nbase > 0);
    for(auto i = 0; i < args_->nbase; ++i){
        auto base = lossLayerFactory(args_, args_->loss);
        base->setup(args_, dict);
        base->setShift(sizeSum);
        sizeSum += base->getSize();
        baseLayers.push_back(base);
    }

    multilabel = baseLayers[0]->isMultilabel();
    std::cout << "  N base: " << args_->nbase << ", output mat size: " << sizeSum << ", multilabel " << multilabel << "\n";
}

real Bagging::loss(const std::vector <int32_t> &input, const std::vector<int32_t>& labels, real lr, Model *model_){
    real lossSum = 0.0;
    real numOfUpdates = 0.0;
    std::string catInput = "&";

    if(args_->bagging < 1.0){
        for(auto i : input)
            catInput += "_" + std::to_string(i);
    }

    for(auto i = 0; i < baseLayers.size(); ++i) {
        if(args_->bagging < 1.0 && hashInput(std::to_string(i) + catInput) < args_->bagging) continue;
        lossSum += baseLayers[i]->loss(labels, lr, model_);
        numOfUpdates += 1.0;
    }

    return lossSum/args_->nbase;
}

void Bagging::findKBest(int32_t top_k, std::vector<std::pair<real, int32_t>>& heap, Vector& hidden, const Model *model_){
    std::unordered_map <int32_t, real> label_freq;
    std::set<int32_t> label_set;

    for(int i=0; i < args_->nbase; i++ ){
        heap.clear();
        baseLayers[i]->findKBest(top_k, heap, hidden, model_);

        for(auto const& value: heap) {
            label_set.insert(value.second);
        }
    }

    heap.clear();    
    for(auto const& value : label_set) label_freq[value] = 0.0;
    
    for(int i=0; i < args_->nbase; i++ ){
        for(auto const& value : label_set){
            real prob = baseLayers[i]->getLabelP(value, hidden, model_);
            label_freq[value] += prob;
        }
    }

    for(const auto& elem: label_freq)
        heap.push_back(std::make_pair(elem.second / ((real)args_->nbase), elem.first));

    std::sort(heap.rbegin(), heap.rend());
    heap.resize(top_k);
}

int32_t Bagging::getSize(){
    return sizeSum;
}

void Bagging::save(std::ostream& out){
    std::cerr << "Saving Bagging layer ...\n";
    for(auto base : baseLayers)
        base->save(out);
}

void Bagging::load(std::istream& in){
    std::cerr << "Loading Bagging layer ...\n";

    for(auto i = 0; i < args_->nbase; ++i){
        auto base = lossLayerFactory(args_, args_->loss);
        base->load(in);
        baseLayers.push_back(base);
    }
}

real Bagging::hashInput(const std::string& str){
    uint32_t h = 2166136261;
    for (size_t i = 0; i < str.size(); i++) {
        h = h ^ uint32_t(str[i]);
        h = h * 16777619;
    }
    uint32_t max = 1 << 24;
    return static_cast<real>(h % max) / max;
}

}
