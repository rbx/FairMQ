/********************************************************************************
 * Copyright (C) 2020 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef FAIR_MQ_ERRORS_H
#define FAIR_MQ_ERRORS_H

#include <stdexcept>

namespace fair
{
namespace mq
{

struct TransportFactoryError : std::runtime_error { using std::runtime_error::runtime_error; };

} /* namespace mq */
} /* namespace fair */

#endif /* FAIR_MQ_ERRORS_H */
