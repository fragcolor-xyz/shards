//
// Created by giova on 8/10/2019.
//

#ifndef CHAINBLOCKS_PRIVATE_FLOW_HPP
#define CHAINBLOCKS_PRIVATE_FLOW_HPP

#include "shared.hpp"

namespace chainblocks
{
static TypesInfo conditionPairInfo = TypesInfo::FromMany();
static TypesInfo conditionSeqInfo = TypesInfo(CBType::Seq, CBTypesInfo(intInfo));
static ParamsInfo condParamsInfo = ParamsInfo(
    ParamsInfo::Param("Conditions", "The conditions to test.", CBTypesInfo(chainTypes)),
    ParamsInfo::Param("Actions", "The actions to perform if the corresponding condition matches.", CBTypesInfo(boolInfo)));
struct Cond
{

};
};

#endif //CHAINBLOCKS_PRIVATE_FLOW_HPP
