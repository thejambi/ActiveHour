var VERSION = "1.0";

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
    "PERSIST_KEY_CLR_GREEN": '' + json.clr_green
  };

  Pebble.sendAppMessage(options,
    function(e) {
      console.log('Settings update successful!');
    },
    function(e) {
      console.log('Settings update failed: ' + JSON.stringify(e));
    });
});




// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log("PebbleKit JS ready!");
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    console.log('AppMessage received! Received message: ' + JSON.stringify(e.payload));
  }                     
);





