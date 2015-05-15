#! /bin/bash
#
# SysDB -- t/coverage.sh
#
# Create a test coverage report for the System DataBase.

set -e

if ! which lcov || ! which genhtml; then
	echo "lcov is required in order to generate a test coverage report" >&2
	exit 1
fi

cd $( dirname "$0" )/..

if test -e t/coverage; then
	echo "t/coverage exists already; please remove manually" >&2
	exit 1
fi

V=$( grep '^PACKAGE_VERSION' Makefile | cut -d' ' -f3 )
if test -z "$V"; then
	echo "Unable to determine package version" >&2
	exit 1
fi

make dist

tmpdir=$( mktemp -d -t )
trap "rm -rf $tmpdir" EXIT

srcdir=$( pwd )

cd $tmpdir
tar --strip-components=1 -zxf "$srcdir"/sysdb-$V.tar.gz
if test -d "$srcdir"/.git/; then
	# copy Git database for version-gen.sh
	cp -a "$srcdir"/.git .

	# reset all files which are not part of the tarball
	git checkout HEAD .gitignore .travis.yml t/cibuild.sh
fi

# rebuild build system to refresh version number, etc.
rm -f version
touch configure.ac && make configure

./configure --enable-gcov CFLAGS="-O0 -g"
make

lcov --base-directory src --directory src --zerocount
make -j10 test || (status=$?; cat t/test-suite.log 2> /dev/null; exit $status)

# old versions of lcov don't support --no-external
lcov --base-directory src --directory src --no-external \
	--capture -o sysdb_coverage.info \
	|| lcov --base-directory src --directory src \
		--capture -o sysdb_coverage.info
for pattern in '*.y' '*.l' \
		'sysdb/scanner.c' 'frontend/scanner.c' 'liboconfig/scanner.c' \
		'parser/scanner.c' 'frontend/grammar.c' 'parser/grammar.c' \
		'liboconfig/parser.c'
do
	lcov --remove sysdb_coverage.info "$pattern" -o sysdb_coverage.info
done

V=$( ./version-gen.sh )
genhtml -o "$srcdir"/t/coverage \
	-t "SysDB $V test coverage" --num-spaces=4 --legend \
	sysdb_coverage.info

