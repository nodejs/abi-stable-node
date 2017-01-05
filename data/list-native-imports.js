// Collects data about functions imported from node.exe by all packages installed under the
// current directory. Currently Windows only. See README.md for details.

const childProc = require('child_process');
const fs = require('fs');
const path = require('path');

const rootDir = process.cwd();

summarizeImports();

function summarizeImports() {
    let summaryImports = [];
    let packageNames = fs.readdirSync(path.join(rootDir, 'node_modules'));
    packageNames.forEach(packageName => {
        let packageImports = getImportsForPackage(packageName);
        packageImports.forEach(packageImport => {
            let summaryImport = summaryImports.find(i => i.signature === packageImport.signature);
            if (summaryImport) {
                summaryImport.packages.push(packageName);
            } else {
                packageImport.packages = [packageName];
                summaryImports.push(packageImport);
            }
        });
    });

    console.log('Pkg count,Package names,Imported name,Imported signature');
    summaryImports.sort((a, b) => a.packages.length > b.packages.length ? -1 : 1).forEach(i => {
        console.log(
            i.packages.length + ',"' +
            i.packages.join(', ') + '","' +
            i.name + '","' +
            i.signature + '"');
    });
}

function getImportsForPackage(packageName) {
    let packageNativeModules = getNativeModulesForPackage(packageName);

    let nativeImports = [];
    packageNativeModules.forEach(nativeModule => {
        let moduleImports = getImportsForNativeModule(nativeModule);
        moduleImports.forEach(moduleImport => {
            if (!nativeImports.find(i => i.signature === moduleImport.signature)) {
                nativeImports.push(moduleImport);
            }
        });
    });
    return nativeImports;
}

function getNativeModulesForPackage(packageName) {
    let packageDir = path.join(rootDir, 'node_modules', packageName);
    return findFiles(packageDir, relativePath => {
        return relativePath.endsWith('.node') &&
            !relativePath.startsWith('node_modules') &&
            relativePath.indexOf('ia32') < 0;
    }).map(relativePath => path.join(packageDir, relativePath));
}

function getImportsForNativeModule(modulePath) {
    const dumpbinExe =
        'C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\bin\\dumpbin.exe';
    const undnameExe =
        'C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\bin\\undname.exe';
    const importRegex = /^ *[0-9A-F]+ +0 +([^ ]+) *$/;
    const undnameRegex = /is :- "([^"]+)"/;
    const fnameRegex = /([\w:~]+)\(/;

    let p = childProc.spawnSync(dumpbinExe, ['/imports:node.exe', modulePath]);
    let lines = p.stdout.toString().split('\r\n');

    let imports = [];
    lines.forEach(line => {
        let m = importRegex.exec(line);
        if (m) {
            let decoratedName = m[1];
            p = childProc.spawnSync(undnameExe, [decoratedName]);
            let output = p.stdout.toString();
            m = undnameRegex.exec(output);
            if (m) {
                let signature = m[1];
                m = fnameRegex.exec(signature);
                let name = (m ? m[1] : signature);
                imports.push({ name, signature });
            }
        }
    });

    return imports;
}

function findFiles(rootDir, filter, relativeDir) {
    let results = [];
    let childNames = fs.readdirSync(path.join(rootDir, relativeDir || ''));
    childNames.forEach(childName => {
        let relativePath = path.join(relativeDir || '', childName);
        let childStats = fs.statSync(path.join(rootDir, relativePath));
        if (childStats.isDirectory()) {
            let childResults = findFiles(rootDir, filter, relativePath);
            results = results.concat(childResults);
        } else {
            if (filter(relativePath)) {
                results.push(relativePath);
            }
        }
    });
    return results;
}
