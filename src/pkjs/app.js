var VERSION = "1.0";

var showWeather = 0;

Pebble.addEventListener('showConfiguration', function(e) {
  Pebble.openURL('https://skudpaisho.com/other/activehour.html?version=' + VERSION);
});

Pebble.addEventListener('webviewclosed', function(e) {
  var json = JSON.parse(decodeURIComponent(e.response));

  // Custom theme colors arrive as "#rrggbb" strings; the watch wants packed ints.
  function hexToInt(hex, fallback) {
    var v = parseInt(('' + hex).replace('#', ''), 16);
    return isNaN(v) ? fallback : v;
  }

  var options = {
    "PERSIST_KEY_DATE": '' + json.date,
    "PERSIST_KEY_STEPS": '' + json.steps,
    "PERSIST_KEY_CLR_BW": '' + json.clr_bw,
    "PERSIST_KEY_CLR_ORANGE": '' + json.clr_orange,
    "PERSIST_KEY_CLR_GREEN": '' + json.clr_green,
    "PERSIST_KEY_CLR_BLUE": '' + json.clr_blue,
    "PERSIST_KEY_CLR_PURPLE": '' + json.clr_purple,
    "PERSIST_KEY_CLR_RED": '' + json.clr_red,
    "PERSIST_KEY_CLR_TEAL": '' + json.clr_teal,
    "PERSIST_KEY_CLR_CUSTOM": '' + json.clr_custom,
    "PERSIST_KEY_CUSTOM_BG": hexToInt(json.custom_bg, 0x000000),
    "PERSIST_KEY_CUSTOM_TIME": hexToInt(json.custom_time, 0xffffff),
    "PERSIST_KEY_CUSTOM_DOT_ACTIVE": hexToInt(json.custom_dot_active, 0xff6a00),
    "PERSIST_KEY_CUSTOM_DOT_DIM": hexToInt(json.custom_dot_dim, 0x555555),
    "PERSIST_KEY_CUSTOM_STEPS": hexToInt(json.custom_steps, 0xaaaaaa),
    "PERSIST_KEY_CUSTOM_DATE": hexToInt(json.custom_date, 0xaaaaaa),
    "PERSIST_KEY_WEATHER": '' + json.weather,
    "PERSIST_KEY_BOLD_TEXT": '' + json.boldText,
    "PERISST_KEY_BOLD_DOTS": '' + json.boldDots,
    "PERSIST_KEY_MINMARKS": '' + json.minmarks,
    "PERSIST_KEY_FITDOTS": '' + json.fitdots
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
  // Open-Meteo: free, no API key, HTTPS, and returns Fahrenheit directly.
  var url = "https://api.open-meteo.com/v1/forecast?latitude=" +
      pos.coords.latitude + "&longitude=" + pos.coords.longitude +
      "&current=temperature_2m&temperature_unit=fahrenheit";

  // Send request to Open-Meteo
  xhrRequest(url, 'GET',
    function(responseText) {
      // responseText contains a JSON object with weather info
      var json = JSON.parse(responseText);

      if (!json || !json.current || typeof json.current.temperature_2m !== 'number') {
        console.log("No weather data in response: " + responseText);
        return;
      }

      // Already in Fahrenheit; just round to a whole degree.
      var temperature = Math.round(json.current.temperature_2m);
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

    if (e.payload.PERSIST_KEY_WEATHER) {
      showWeather = e.payload.PERSIST_KEY_WEATHER;
    }
    console.log("showWeather " + showWeather);

    // Fetch on any weather signal: the initial handshake AND the watch's
    // 30-minute refresh both land here. getWeather() guards on showWeather.
    if (showWeather) {
      getWeather();
    }
  }
);


