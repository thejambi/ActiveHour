/* ---------------------------------------------------------------- config */

// Vendored from pebble-clay 1.0.4 (MIT) — the npm package declares a platform
// allowlist that predates flint/gabbro and ships only stub binaries, so we
// carry its self-contained JS bundle directly. See vendor/LICENSE-pebble-clay.txt.
var Clay = require('./vendor/pebble-clay');
var clayConfig = require('./config');
var messageKeys = require('message_keys');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var showWeather = 0;

Pebble.addEventListener('showConfiguration', function(e) {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) {
    console.log('Config closed without saving');
    return;
  }

  var dict = clay.getSettings(e.response);

  // The watch stores the clock font as radio-style booleans (all false =
  // Bitham). Translate the page's single select and drop the virtual key.
  var font = dict[messageKeys.CLOCK_FONT];
  delete dict[messageKeys.CLOCK_FONT];
  dict[messageKeys.PERSIST_KEY_FONT_MONT]   = (font === 'mont')   ? 1 : 0;
  dict[messageKeys.PERSIST_KEY_FONT_ROBOTO] = (font === 'roboto') ? 1 : 0;
  dict[messageKeys.PERSIST_KEY_FONT_LECO]   = (font === 'leco')   ? 1 : 0;

  // Same for the color theme presets (all false would mean B&W on-watch, but
  // the page always sends exactly one preset true, or custom).
  var theme = dict[messageKeys.THEME];
  delete dict[messageKeys.THEME];
  dict[messageKeys.PERSIST_KEY_CLR_BW]     = (theme === 'bw')     ? 1 : 0;
  dict[messageKeys.PERSIST_KEY_CLR_ORANGE] = (theme === 'orange') ? 1 : 0;
  dict[messageKeys.PERSIST_KEY_CLR_GREEN]  = (theme === 'green')  ? 1 : 0;
  dict[messageKeys.PERSIST_KEY_CLR_BLUE]   = (theme === 'blue')   ? 1 : 0;
  dict[messageKeys.PERSIST_KEY_CLR_PURPLE] = (theme === 'purple') ? 1 : 0;
  dict[messageKeys.PERSIST_KEY_CLR_RED]    = (theme === 'red')    ? 1 : 0;
  dict[messageKeys.PERSIST_KEY_CLR_TEAL]   = (theme === 'teal')   ? 1 : 0;
  dict[messageKeys.PERSIST_KEY_CLR_CUSTOM] = (theme === 'custom') ? 1 : 0;

  // Custom theme colors must reach the watch as packed ints; depending on
  // Clay's color component internals they can surface as hex strings.
  [messageKeys.PERSIST_KEY_CUSTOM_BG,
   messageKeys.PERSIST_KEY_CUSTOM_TIME,
   messageKeys.PERSIST_KEY_CUSTOM_DOT_ACTIVE,
   messageKeys.PERSIST_KEY_CUSTOM_DOT_DIM,
   messageKeys.PERSIST_KEY_CUSTOM_STEPS,
   messageKeys.PERSIST_KEY_CUSTOM_DATE].forEach(function(k) {
    if (typeof dict[k] === 'string') {
      dict[k] = parseInt(dict[k].replace('#', '').replace('0x', ''), 16) || 0;
    }
  });

  // Toggles come back as booleans; the watch reads ints.
  Object.keys(dict).forEach(function(k) {
    if (typeof dict[k] === 'boolean') {
      dict[k] = dict[k] ? 1 : 0;
    }
  });

  Pebble.sendAppMessage(dict,
    function(e) {
      console.log('Settings update successful!');
    },
    function(e) {
      console.log('Settings update failed: ' + JSON.stringify(e));
    });
});

/* ---------------------------------------------------------------- weather */

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

  xhrRequest(url, 'GET',
    function(responseText) {
      var json = JSON.parse(responseText);

      if (!json || !json.current || typeof json.current.temperature_2m !== 'number') {
        console.log("No weather data in response: " + responseText);
        return;
      }

      var temperature = Math.round(json.current.temperature_2m);
      console.log("Temperature is " + temperature);

      Pebble.sendAppMessage({ "KEY_TEMPERATURE": temperature },
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

    Pebble.sendAppMessage({ "KEY_JSREADY": 1 },
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
