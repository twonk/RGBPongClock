//Man in the Middle weather node.js script for RGBPongClock
// Andrew Holmes @PongClock
//Update the URLS at the bottom of the script for your home town.
//RGPPong Clock opens socket to port 1337, sends W
//Script then
//1. Retrieves forecast from OpenWeatherMap.org 
//2. Parses, and returns data to RPGPongClock in the format
//  <temp~id~desc~temp~id~desc~temp,id,desc~...>
//  <..Current...|....Day1....|....Day2....|...>
//

var net = require('net');

var server = net.createServer(function (socket) {
  socket.setEncoding("utf8");
  socket.addListener("connect", function () {});
  socket.addListener("data", function (data) {
   if (data.substring(0,1)=='W') {
      var responseParts = [];
      http.get(weather_current_options, function (res) {
        res.setEncoding("utf8");
        res.on("data", function (chunk) {
          responseParts.push(chunk);
        });
        res.on("end", function () {
          //now send your complete response
          responseParts.join('');
          var weather = {};
          weather = JSON.parse(responseParts);
          socket.write("<");
          socket.write(niceOutput(Math.round(weather.main.temp)));
          socket.write(niceOutput(weather.weather[0].id));
          socket.write(niceOutput(weather.weather[0].main));

          var forecast_parts=[];
          http_forecast.get(weather_forecast_options, function (resp){
            resp.setEncoding("utf8");
            resp.on("data",function (chunks){
              forecast_parts.push(chunks);
            });
            resp.on("end", function() {
              forecast_parts.join('');
              //console.log(forecast_parts);
              var forecast = {};
              forecast = JSON.parse(forecast_parts);
              var days = [];
              for(var i=0; i<forecast.list.length ; i++){
                var day = {};
                day = forecast.list[i];
                socket.write(niceOutput(Math.round(day.temp.day)));
                socket.write(niceOutput(day.weather[0].id));
                socket.write(niceOutput(day.weather[0].main));
              }
              socket.end('>');
            });
          }).on('error', function(e){
            socket.write('<>');
            console.log('Error: '+e.message);
          });
        });
        //socket.write(JSON.stringify(res.data));
      }).on('error', function (e) {
        socket.write('<>');
        console.log('ERROR: ' + e.message);
      });
    };
  });
});
function niceOutput(s){
  return (JSON.stringify(s).replace( /\"/g,'')+'~');
}

var http = require('http');
var http_forecast = require('http');
var weather_current_options = {
  host: 'api.openweathermap.org',
  path: '/data/2.5/weather?q=Peterborough,uk&units=metric'
};
var weather_forecast_options = {
  host: 'api.openweathermap.org',
  path: '/data/2.5/forecast/daily?q=Peterborough,uk&units=metric&cnt=7'
};

server.listen(1337, '0.0.0.0');
