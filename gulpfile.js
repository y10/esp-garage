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
 
const fs = require('fs');
const path = require('path');
const gulp = require('gulp');
const plumber = require('gulp-plumber');
const htmlmin = require('gulp-htmlmin');
const cleancss = require('gulp-clean-css');
const uglify = require('gulp-uglify');
const gzip = require('gulp-gzip');
const del = require('del');
const useref = require('gulp-useref');
const gulpif = require('gulp-if');
const inline = require('gulp-inline');
const inlineImages = require('gulp-css-base64');
const favicon = require('gulp-base64-favicon');
const gutil = require('gulp-util');
const foreach = require('gulp-flatmap');

 
/* Clean destination folder */
gulp.task('clean', function() {
    return del(['data/***']);
});
 

gulp.task('gzip', function() {
    return gulp.src('html/**/*.*')
        .pipe(gzip())
        .pipe(gulp.dest('data'));
});

gulp.task("favicons", function () {
    return gulp.src("html/icon.png").pipe(favicons({
        appName: "Switch",
        appDescription: "Switch & Timer",
        path: "/favicons/"
    }))
    .on("error", gutil.log)
    .pipe(gulp.dest("data/favicons"));
});

gulp.task('buildfs_inline', ['clean', 'gzip'], function() {
    return gulp.src('html/*.html')
        .pipe(favicon())
        .pipe(inline({
            base: 'html/',
            js: uglify,
            css: [cleancss, inlineImages],
            disabledTypes: ['svg', 'img']
        }))
        .pipe(htmlmin({
            collapseWhitespace: true,
            removeComments: true,
            minifyJS: true,
            minifyCSS: true
        }))
        .pipe(gzip())
        .pipe(gulp.dest('data'));
})

gulp.task('buildfs_embeded', ['buildfs_inline'], function() {
    
    return gulp.src('data/**/*.*')
    .pipe(foreach(function(stream, file){
    
      var data = file.contents;
      if(data != null)
      {
          var filename =  path.basename(file.path);
          var wstream = fs.createWriteStream(file.path + '.h');
          wstream.on('error', function (err) {
              gutil.log(err);
          });
      
          wstream.write('const uint8_t GARAGE_' + filename.replace(/\./g, '_').replace(/-/g, '_').toUpperCase() +'[] PROGMEM = {')
      
          for (i=0; i<data.length; i++) {
              if (i % 1000 == 0) wstream.write("\n");
              wstream.write('0x' + ('00' + data[i].toString(16)).slice(-2));
              if (i<data.length-1) wstream.write(',');
          }
          
          wstream.write('\n};')
          wstream.end();
      }
      return stream;
    }));
    
});

gulp.task('default', ['buildfs_embeded']);