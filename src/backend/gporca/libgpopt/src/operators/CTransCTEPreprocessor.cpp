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
 * CDedupSupersetPreprocessor.cpp
 *
 * IDENTIFICATION
 *	  src/backend/gporca/libgpopt/src/operators/CTransCTEPreprocessor.cpp
 *
 *-------------------------------------------------------------------------
 */
#include "gpopt/operators/CTransCTEPreprocessor.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CLogicalCTEAnchor.h"
#include "gpopt/operators/CLogicalCTEConsumer.h"
#include "gpopt/operators/CLogicalCTEProducer.h"
#include "gpopt/operators/CLogicalProject.h"

using namespace gpopt;

CExpression *
CTransCTEPreprocessor::TransToCTE(CMemoryPool *, CExpression *pexpr,
								  CExpressionArray *, ColRefToExprMap *dict)
{
	for (ULONG ul1 = 0; ul1 < pexpr->Arity(); ul1++)
	{
		CExpression *drgpexpr1 = (*pexpr)[ul1];
		for (ULONG ul2 = ul1 + 1; ul2 < pexpr->Arity(); ul2++)
		{
			CExpression *drgpexpr2 = (*pexpr)[ul2];
			ApproximateEquals(drgpexpr1, drgpexpr2, dict);
		}
	}
	return pexpr;
}

BOOL
CTransCTEPreprocessor::ApproximateEquals(CExpression *lhsExpr,
										 CExpression *rhsExpr,
										 ColRefToExprMap *dict)
{
	GPOS_CHECK_STACK_SIZE;
	COperator *popLeft = nullptr;
	COperator *popRight = nullptr;

	if (nullptr == lhsExpr || nullptr == rhsExpr)
	{
		return lhsExpr == nullptr && rhsExpr == nullptr;
	}

	popLeft = lhsExpr->Pop();
	popRight = rhsExpr->Pop();

	if (!popLeft->ApproximateMatches(popRight, dict))
	{
		return false;
	}

	BOOL fEqual = true;
	const ULONG arity = lhsExpr->Arity();

	if (rhsExpr->Arity() != arity)
		return false;

	for (ULONG ul = 0; fEqual && ul < arity; ul++)
	{
		// child must be at the same position in the other expression
		fEqual = ApproximateEquals((*lhsExpr)[ul], (*rhsExpr)[ul], dict);
	}


	return fEqual;
}

BOOL
CTransCTEPreprocessor::CollectColRefDictionary(CExpression *pexpr, ColRefToExprMap *dict)
{
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(nullptr != pexpr);
	BOOL success = true;
	COperator *pop = pexpr->Pop();

	// setop force remap colref, does not support it yet
	if (CUtils::FLogicalSetOp(pop))
		return false;

	const ULONG arity = pexpr->Arity();

	for (ULONG ul = 0; ul < arity; ul++)
		success =  success && CollectColRefDictionary((*pexpr)[ul], dict);
	
	if (pop->Eopid() == COperator::EopScalarProjectElement)
	{
		CExpression *drgexpr = (*pexpr)[0];
		CScalarProjectElement *popProjElement = CScalarProjectElement::PopConvert(pop);
		CColRef *cr = popProjElement->Pcr();

		drgexpr->AddRef();
		dict->Insert(cr, drgexpr);
	}
	
	return success;
}

CExpression *
CTransCTEPreprocessor::PexprPreprocess(CMemoryPool *mp, CExpression *pexpr)
{
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(nullptr != mp);
	GPOS_ASSERT(nullptr != pexpr);

	CExpressionArray *subqpexpr = pexpr->PsubqueryPexpr();
	ColRefToExprMap *dict = GPOS_NEW(mp) ColRefToExprMap(mp);
	CExpressionArrays *subqcommexprs = GPOS_NEW(mp) CExpressionArrays(mp);
	ULONG ulSubqueries;

	if (subqpexpr == nullptr)
		goto trans_cte_failed;

	ulSubqueries = subqpexpr->Size();


	// collect colref-operator map
	for (ULONG ul = 0; ul < ulSubqueries; ul++)
	{
		if (!CollectColRefDictionary((*subqpexpr)[ul], dict))
			goto trans_cte_failed;
	}

	// collect approximate equal subqueries into one group
	for (ULONG ul1 = 0; ul1 < ulSubqueries; ul1++)
	{
		BOOL match = false;
		ULONG ul2 = 0;
		CExpression *subqexpr = (*subqpexpr)[ul1];
		const ULONG ulsubcommexprs = subqcommexprs->Size();

		for (; ul2 < ulsubcommexprs && !match; ul2++)
		{
			CExpressionArray *commexprs = (*subqcommexprs)[ul2];

			CExpression *commexpr = (*commexprs)[0];

			if (ApproximateEquals(commexpr, subqexpr, dict))
			{
				match = true;

				subqexpr->AddRef();
				commexprs->Append(subqexpr);
			}
		}

		// if current expr does not find approximate equal, create a new group
		if (!match)
		{
			CExpressionArray *newcommexpr = GPOS_NEW(mp) CExpressionArray(mp);

			subqexpr->AddRef();
			newcommexpr->Append(subqexpr);

			subqcommexprs->Append(newcommexpr);
		}
	}

	// generate cte producers for each group
	ulSubqueries = subqcommexprs->Size();
	for (ULONG ul = 0; ul < ulSubqueries; ul++)
	{
		CExpressionArray *commexprs = (*subqcommexprs)[ul];
		if (commexprs->Size() > 1)
			pexpr = TransToCTE(mp, pexpr, commexprs, dict);
	}

trans_cte_failed:
	subqcommexprs->Release();
	dict->Release();

	pexpr->AddRef();
	return pexpr;
//	COperator *pop = pexpr->Pop();
//	pop->AddRef();
//	return GPOS_NEW(mp) CExpression(mp, pop, pdrgpexpr);
}