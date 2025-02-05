#include <gtest/gtest.h>

#include "common.h"
#include <cmr/separation.h>

TEST(Separation, OneSum)
{
  CMR* cmr = NULL;
  ASSERT_CMR_CALL( CMRcreateEnvironment(&cmr) );

  {
    CMR_CHRMAT* first = NULL;
    ASSERT_CMR_CALL( stringToCharMatrix(cmr, &first, "5 5 "
      " 1  1  0  0  0 "
      " 1  0  1 -1  1 "
      " 0 -1  1  0 -1 "
      " 0  0 -1  1  0 "
      " 0  1  1  0  1 "
    ) );
    CMR_CHRMAT* second = NULL;
    ASSERT_CMR_CALL( stringToCharMatrix(cmr, &second, "5 5 "
      " 1 -1  1  0  0 "
      " 1  1  1  1 -1 "
      " 0  0 -1  0  1 "
      " 1  0  0 -1  0 "
      " 0  1  0  0  1 "
    ) );

    CMR_CHRMAT* check = NULL;
    ASSERT_CMR_CALL( stringToCharMatrix(cmr, &check, "10 10 "
      " 1  1  0  0  0  0  0  0  0  0 "
      " 1  0  1 -1  1  0  0  0  0  0 "
      " 0 -1  1  0 -1  0  0  0  0  0 "
      " 0  0 -1  1  0  0  0  0  0  0 "
      " 0  1  1  0  1  0  0  0  0  0 "
      " 0  0  0  0  0  1 -1  1  0  0 "
      " 0  0  0  0  0  1  1  1  1 -1 "
      " 0  0  0  0  0  0  0 -1  0  1 "
      " 0  0  0  0  0  1  0  0 -1  0 "
      " 0  0  0  0  0  0  1  0  0  1 "
    ) );

    CMR_CHRMAT* onesum = NULL;
    ASSERT_CMR_CALL( CMRoneSum(cmr, first, second, &onesum) );

    ASSERT_TRUE( CMRchrmatCheckEqual(onesum, check) );

    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &onesum) );
    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &check) );
    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &second) );
    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &first) );  
  }

  ASSERT_CMR_CALL( CMRfreeEnvironment(&cmr) );
}

TEST(Separation, TwoSum)
{
  CMR* cmr = NULL;
  ASSERT_CMR_CALL( CMRcreateEnvironment(&cmr) );

  {
    CMR_CHRMAT* first = NULL;
    ASSERT_CMR_CALL( stringToCharMatrix(cmr, &first, "5 5 "
      " 1  1  0  0  0 "
      " 1  0  1 -1  1 " /* marker row */
      " 0 -1  1  0 -1 "
      " 0  0 -1  1  0 "
      " 0  1  1  0  1 "
    ) );
    CMR_CHRMAT* second = NULL;
    ASSERT_CMR_CALL( stringToCharMatrix(cmr, &second, "5 5 "
      " 1 -1  1  0  0 "
      " 1  1  1  1 -1 "
      " 0  0 -1  0  1 "
      " 1  0  0 -1  0 "
      " 0  1  0  0  1 "
      /*      ^
       * marker column */
    ) );

    CMR_CHRMAT* check = NULL;
    ASSERT_CMR_CALL( stringToCharMatrix(cmr, &check, "9 9 "
      " 1  1  0  0  0  0  0  0  0 "
      " 0 -1  1  0 -1  0  0  0  0 "
      " 0  0 -1  1  0  0  0  0  0 "
      " 0  1  1  0  1  0  0  0  0 "
      " 1  0  1 -1  1  1 -1  0  0 "
      " 1  0  1 -1  1  1  1  1 -1 "
      "-1  0 -1  1 -1  0  0  0  1 "
      " 0  0  0  0  0  1  0 -1  0 "
      " 0  0  0  0  0  0  1  0  1 "
    ) );

    CMR_CHRMAT* twosum = NULL;
    ASSERT_CMR_CALL( CMRtwoSum(cmr, first, second, CMRrowToElement(1), CMRcolumnToElement(2), &twosum) );

    CMRchrmatPrintDense(cmr, twosum, stdout, '0', false);

    ASSERT_TRUE( CMRchrmatCheckEqual(twosum, check) );

    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &twosum) );
    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &check) );
    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &second) );
    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &first) );
  }

  {
    CMR_CHRMAT* first = NULL;
    ASSERT_CMR_CALL( stringToCharMatrix(cmr, &first, "5 5 "
      " 1  1  0  0  0 "
      " 1  0  1 -1  1 "
      " 0 -1  1  0 -1 "
      " 0  0 -1  1  0 "
      " 0  1  1  0  1 "
      /*            ^
       * marker column */
    ) );
    CMR_CHRMAT* second = NULL;
    ASSERT_CMR_CALL( stringToCharMatrix(cmr, &second, "5 5 "
      " 1 -1  1  0  0 " /* marker row */
      " 1  1  1  1 -1 "
      " 0  0 -1  0  1 "
      " 1  0  0 -1  0 "
      " 0  1  0  0  1 " 
    ) );

    CMR_CHRMAT* check = NULL;
    ASSERT_CMR_CALL( stringToCharMatrix(cmr, &check, "9 9 "
      " 1  1  0  0  0  0  0  0  0 "
      " 1  0  1 -1  1 -1  1  0  0 "
      " 0 -1  1  0 -1  1 -1  0  0 "
      " 0  0 -1  1  0  0  0  0  0 "
      " 0  1  1  0  1 -1  1  0  0 "
      " 0  0  0  0  1  1  1  1 -1 "
      " 0  0  0  0  0  0 -1  0  1 "
      " 0  0  0  0  1  0  0 -1  0 "
      " 0  0  0  0  0  1  0  0  1 " 
    ) );

    CMR_CHRMAT* twosum = NULL;
    ASSERT_CMR_CALL( CMRtwoSum(cmr, first, second, CMRcolumnToElement(4), CMRrowToElement(0), &twosum) );

    CMRchrmatPrintDense(cmr, twosum, stdout, '0', false);

    ASSERT_TRUE( CMRchrmatCheckEqual(twosum, check) );

    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &twosum) );
    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &check) );
    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &second) );
    ASSERT_CMR_CALL( CMRchrmatFree(cmr, &first) );
  }

  ASSERT_CMR_CALL( CMRfreeEnvironment(&cmr) );
}
