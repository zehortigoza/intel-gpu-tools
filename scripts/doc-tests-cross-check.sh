#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Copyright © 2024 Intel Corporation
#
# copy this script to folder as it creates lots of output files
# igt tests should be reachable from ../tests/ path
# for example, when you are in main igt folder:
#
# mkdir out
# cp scripts/doc-cross-check.sh out/
# cd out
# ./doc-cross-check.sh

# prepare list of .c files for checks

TESTLISTIGT=".testfiles"

ls -1 ../tests/*c        > $TESTLISTIGT
ls -1 ../tests/intel/*c >> $TESTLISTIGT

TESTLIST=`cat $TESTLISTIGT`
if [ $? -ne 0 ]; then
	echo "Error: Could not read test lists"
	exit 99
fi

grep_test ()
{
	local test
	local myout

	test=$1
	myout=$2

	echo "running: $myout file: $test"
	grep $myout: $test | sed -e "s/...$2: //" \
		| tr '[:upper:]' '[:lower:]' | sed -e 's/, /\n/g' \
		| sed -e 's/ /Y/g' >> b.$myout
	grep -r $myout: ../tests/* >> n.$myout
}


one_scan_dir ()
{
	rm b.$1
	rm n.$1

	for nfile in $TESTLIST; do
		grep_test $nfile $1
	done

	sort -u < b.$1 > c.$1

	rm .tmp_one_scan
	mv n.$1 .tmp_one_scan
	sort -u < .tmp_one_scan > n.$1
}

scan_dirs ()
{
	rm a.columns

	for todo in $@; do
		echo $todo >> a.columns
		echo "Scanning name: $todo"
		one_scan_dir $todo
	done
}

check_test ()
{
	TWORDS=`cat c.$1`
	if [ $? -ne 0 ]; then
		echo "Error: Could not read c.$1"
		exit 99
	fi

#	cat c.$2 c.$3 > w.$1
# cat c.* except c.$1 is in w.$1

	echo "=============================================="
	echo "checking: $1"
	echo "=============================================="

	rm .tmp_check

	for test in $TWORDS; do
		grep -i -w "$test" w.$1 >> .tmp_check
	done

	sort -u < .tmp_check > e.$1
}

# will use .tmpcols and create w.$1
# this is complement of all columns exept $1
make_w ()
{
	WMK=`cat .tmpcols`
	if [ $? -ne 0 ]; then
		echo "Error: Could not read .tmpcols"
		exit 99
	fi

	rm .tmp_make_w
	for one_rec in $WMK; do
		cat c.$one_rec >> .tmp_make_w
	done

	sort -u < .tmp_make_w > w.$1
}

# will use a.columns to cross-check
check_all ()
{
	COL=`cat a.columns`
	if [ $? -ne 0 ]; then
		echo "Error: Could not read c.$1"
		exit 99
	fi

	for one_col in $COL; do
		grep -v $one_col a.columns > .tmpcols
		make_w $one_col
		check_test $one_col
	done
}

echo "=============================================="
echo "scanning..."
echo "=============================================="
# Category/Sub-category/Functionality/Feature
# scan_dirs Category Feature Sub-category

# Mega feature / Category / Sub-category / Functionality
# Mega-feature is not used now
scan_dirs Category Sub-category Functionality

echo "=============================================="
echo "checking..."
echo "=============================================="
check_all

echo "=============================================="
echo "Columns are in c.* files"
echo "=============================================="
wc -l c.[A-Za-z]*
echo "=============================================="
echo "Results (possible conflicts) are in e.* files"
echo "=============================================="
wc -l e.[A-Za-z]*

echo ""
echo "=============================================="
echo "example useage: take a word from e.* file and grep it in c*"
echo "grep -i word c.*"
echo "to see where it is used in tests, use:"
echo "grep -i word n.*"
echo "=============================================="

