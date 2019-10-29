/*
 
ESP8266 file system builder with PlatformIO support
 
Copyright (C) 2016 by Xose Pérez <xose dot perez at gmail dot com>
 
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
 
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
*/
 
// -----------------------------------------------------------------------------
// File system builder
// -----------------------------------------------------------------------------
 
const gulp = require('gulp');
const through = require('through2');

const htmlmin = require('gulp-htmlmin');
const gzip = require('gulp-gzip');
const del = require('del');
const inline = require('gulp-inline');
const inlineImages = require('gulp-css-base64');
const favicon = require('gulp-base64-favicon');
const crass = require('gulp-crass');
const path = require('path');

var makeHeader = function(debug) {

    return through.obj(function (source, encoding, callback) {

        var parts = source.path.split(path.sep);
        var filename = parts[parts.length - 1];
        var safename = filename.split('.').join('_').split('-').join('_').toUpperCase();

        // Generate output
        var output = '';
        output += 'const uint8_t GARAGE_' + safename + '[] PROGMEM = {';
        for (var i=0; i<source.contents.length; i++) {
            if (i > 0) { output += ','; }
            if (0 === (i % 20)) { output += '\n'; }
            output += '0x' + ('00' + source.contents[i].toString(16)).slice(-2);
        }
        output += '\n};';

        // clone the contents
        var destination = source.clone();
        destination.path = source.path + '.h';
        destination.contents = Buffer.from(output);

        if (debug) {
            console.info('Image ' + filename + ' \tsize: ' + source.contents.length + ' bytes');
        }

        callback(null, destination);
    });

};
 
/* Clean destination folder */
gulp.task('clean', function() {
    return del(['data/***']);
});
 

gulp.task('gzip', function() {
    return gulp.src('html/**/*.*')
        .pipe(gzip())
        .pipe(gulp.dest('data'));
});

gulp.task('html', function() {
    return gulp.src('html/*.html')
        .pipe(favicon())
        .pipe(inline({
            base: 'html/',
            js: [],
            css: [crass, inlineImages],
            disabledTypes: ['svg', 'img']
        }))
        .pipe(htmlmin({
            collapseWhitespace: true,
            removeComments: true,
            minifyCSS: true,
            minifyJS: true
        }))
        .pipe(gzip())
        .pipe(gulp.dest('data'))
})

gulp.task('headers', function() {
    
    return gulp.src('data/**/*.*').pipe(makeHeader(true)).pipe(gulp.dest('data'))
  
});


gulp.task('default', gulp.series('clean', 'gzip', 'html', 'headers'));