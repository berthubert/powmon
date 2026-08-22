project('powmon', 'cpp', default_options : ['cpp_std=c++23'])

sqlitedep = dependency('sqlite3')
thread_dep = dependency('threads')
json_dep = dependency('nlohmann_json')
fmt_dep = dependency('fmt', version: '>10')
sqlitewriter_dep = dependency('sqlitewriter', static: true)
doctest_dep=dependency('doctest')
argparse_dep = dependency('argparse', version: '>=3')
cpphttplib = dependency('cpp-httplib', default_options: 'cpp-httplib_brotli=disabled')

executable('powmon', 'p1parse.cc', 'money.cc',
	dependencies: [sqlitedep, json_dep, fmt_dep, sqlitewriter_dep, cpphttplib, argparse_dep])


#executable('testrunner', 'testrunner.cc', 'support.cc',  'siphash.cc', 'search.cc', 'meta.cc',
#dependencies: [sqlitedep, json_dep, fmt_dep, sqlitedep, sqlitewriter_dep, doctest_dep, cpphttplib, simplesockets_dep])

