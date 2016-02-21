var VERSION = "1.0";
var myAPIKey = "f50feb3d24e97418da7764008a110a77";

var showWeather = 0;

Pebble.addEventListener('showConfiguration', function(e) {
  Pebble.openURL('http://calebhugo.com/zach/activehour/activehour.html?version=' + VERSION);
});

Pebble.addEventListener('webviewclosed', function(e) {
  var json = JSON.parse(decodeURIComponent(e.response));

  var options = {
    "PERSIST_KEY_DATE": '' + json.date,
    "PERSIST_KEY_STEPS": '' + json.steps,
    "PERSIST_KEY_CLR_BW": '' + json.clr_bw,
    "PERSIST_KEY_CLR_ORANGE": '' + json.clr_orange,
    "PERSIST_KEY_CLR_GREEN": '' + json.clr_green,
    "PERSIST_KEY_CLR_BLUE": '' + json.clr_blue,
    "PERSIST_KEY_WEATHER": '' + json.weather,
    "PERSIST_KEY_BOLD_TEXT": '' + json.boldText,
    "PERISST_KEY_BOLD_DOTS": '' + json.boldDots
  };

  Pebble.sendAppMessage(options,
    function(e) {
      console.log('Settings update successful!');
    },
    function(e) {
      console.log('Settings update failed: ' + JSON.stringify(e));
    });
});









var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};

function locationSuccess(pos) {
  // Construct URL
  var url = "http://api.openweathermap.org/data/2.5/weather?lat=" +
      pos.coords.latitude + "&lon=" + pos.coords.longitude + '&appid=' + myAPIKey;

  // Send request to OpenWeatherMap
  xhrRequest(url, 'GET', 
    function(responseText) {
      // responseText contains a JSON object with weather info
      var json = JSON.parse(responseText);

      // Temperature in Kelvin requires adjustment
      var temperature = Math.round((json.main.temp - 273.15) * 1.8000) + 32;
      console.log("Temperature is " + temperature);

      // Assemble dictionary using our keys
      var dictionary = {
        "KEY_TEMPERATURE": temperature
      };

      // Send to Pebble
      Pebble.sendAppMessage(dictionary,
        function(e) {
          console.log("Weather info sent to Pebble successfully!");
        },
        function(e) {
          console.log("Error sending weather info to Pebble!");
        }
      );
    }      
  );
}

function locationError(err) {
  console.log("Error requesting location!");
}

function getWeather() {
  if (showWeather == 1) {
    console.log("getting weather");

    navigator.geolocation.getCurrentPosition(
      locationSuccess,
      locationError,
      {timeout: 15000, maximumAge: 60000}
    );
    
  }
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log("PebbleKit JS ready!");

    // Send to Pebble
    // Assemble dictionary using our keys
      var dictionary = {
        "KEY_JSREADY": 1
      };
      Pebble.sendAppMessage(dictionary,
        function(e) {
          console.log("JS Ready info sent to Pebble successfully!");
        },
        function(e) {
          console.log("Error sending JS Ready info to Pebble!");
        }
      );
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    console.log('AppMessage received! Received message: ' + JSON.stringify(e.payload));

    var oldValue = showWeather;
    if (e.payload.PERSIST_KEY_WEATHER) {
      showWeather = e.payload.PERSIST_KEY_WEATHER;
    }
    console.log("showWeather " + showWeather);
    
    if (showWeather && !oldValue) {
      getWeather();
    }
  }                     
);


