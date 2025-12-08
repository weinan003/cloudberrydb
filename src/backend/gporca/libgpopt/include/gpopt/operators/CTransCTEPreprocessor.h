/*-------------------------------------------------------------------------
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 * CTransCTEPreprocessor.h
 *
 * IDENTIFICATION
 *	  src/backend/gporca/libgpopt/include/gpopt/operators/CTransCTEPreprocessor.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef GPOPT_CTransCTEPreprocessor_H
#define GPOPT_CTransCTEPreprocessor_H

#include "gpos/base.h"
#include "gpos/memory/set.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/operators/CExpression.h"

namespace gpopt
{

class CTransCTEPreprocessor
{
private:
	static BOOL CollectColRefDictionary(CExpression *pexpr,
										ColRefToExprMap *dict);

	static CExpression *TransToCTE(CMemoryPool *mp, CExpression *pexpr,
								   CExpressionArray *commexprs,
								   ColRefToExprMap *dict);

public:
	CTransCTEPreprocessor(const CTransCTEPreprocessor &) = delete;
	
	static CExpression *PexprPreprocess(CMemoryPool *mp, CExpression *pexpr);

	static BOOL ApproximateEquals(CExpression *lhsExpr,
								  CExpression *rhsExpr,
								  ColRefToExprMap *dict);
};

}  // namespace gpopt


#endif	// GPOPT_CTransCTEPreprocessor_H
