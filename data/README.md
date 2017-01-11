# Native API imports survey data

This directory contains data about imported native APIs for the most depended-upon NPM packages
with native modules, along with the script used to generate/update the API data. The intention is
to use this data to track and prioritize NAPI coverage of the most-used native APIs.

## TopPackagesWithNativeModules.csv
This data is somewhat collected using the [`npm-pkg-top`](https://github.com/nodejitsu/npm-pkg-top)
tool, but manually edited to remove false positives. `npm-pkg-top` does not do a perfect job of
listing only packages with native modules: when querying with type 'binary' (the default) it
actually includes any packages that have a preinstall, install, or postinstall script. (It uses the
[skimdb needBuild view](https://github.com/nodejitsu/npm-pkg-top/blob/master/index.js#L20), which
[filters packages on the existence of those scripts](https://github.com/npm/npm-registry-couchapp/blob/master/registry/views.js#L263).)
While `node_gyp` is probably the most common reason for a package to have an install script, there
are many packages that have other kinds of install scripts.

## TopNativeImports.csv
This data is collected using the `list-native-imports.js` script in this directory.

The current data includes 31 of the top packages having native modules, by number of dependencies.
Not included are several top packages that failed to install: fsevents, pty.js, newrelic, mdns,
java, i2c, windows.foundation, usb, time, v8-profiler, i2c-bus, mapnik, oracledb, grpc

<table>
<tr><th>Column</th><th>Description</th></tr>
<tr><td>Pkg count</td><td>
The number of packages that imported this native API. (A count of the package names in the next
column.)
</td></tr>
<tr><td>Package names</td><td>
List of packages (NPM package IDs) that imported this native API.
</td></tr>
<tr><td>Imported name</td><td>
Simple name of the native API, including namespace if applicable.<p/>
This column may contain duplicates when there are multiple APIs (overloads) with the same name.
</td></tr>
<tr><td>Imported signature</td><td>
Full C++ signature of the native API, for C++ imports. For C imports (such as libuv APIs), this is
just the same as the previous column because C APIs are exported and imported as simple names.<p/>
Note only 64-bit modules are counted. When a signature includes `__ptr64`, assume that the
equivalent 32-bit API is imported by a 32-bit build of the same module.
</td></tr>
</table>

### Sorting & filtering
By default the data is sorted by the **Pkg count** column, based on the assumption that APIs
imported by the most packages are the highest priority.

To filter the view to see only native APIs imported by a single package, use the following steps:
 1. Open the CSV file in Excel.
 2. Select the first (header) row and turn on filtering for the headers:
    **Ctrl+Shift+L**, or the **Filter** button on the **Data** ribbon.
 3. Click the drop-down arrow for the **Package names** column and enter the desired package name
    in the filter search box.

## Data collection procedure

Prerequisite: Windows with Visual Studio 2015 (including C++ tools). Currently the
`list-native-imports.js` script is Windows-only because it makes use of a couple VC tools for
inspecting binaries: `dumpbin.exe` and `undname.exe`.

 1. Create a new empty Node.js project with `npm init`.
 2. Install all targeted packages containing native modules. Yes, unfortunately this process
    requires installing and *compiling* all the native modules, because there's no other good
    way to determine precisely what native APIs are imported by the native modules.
    ```
    npm install node-sass sqlite3 bcrypt ...
    ```
 3. Run the `list-native-imports.js` script. This writes CSV data to `stdout`; redirect to a `.CSV`
    file to save it.
