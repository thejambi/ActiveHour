// Clay configuration for ActiveHour.
//
// Two items here use VIRTUAL message keys (THEME, CLOCK_FONT) that exist only
// so Clay can render a single select for what the watch stores as radio-style
// booleans. index.js translates them into the real per-option keys and deletes
// the virtual ones before anything is sent — they must never reach the watch.

module.exports = [
  {
    type: 'heading',
    defaultValue: 'ActiveHour'
  },
  {
    type: 'text',
    defaultValue: 'One dot per minute of the past hour — the more you moved ' +
                  'that minute, the more dots stack outward.'
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Display' },
      {
        type: 'toggle',
        messageKey: 'PERSIST_KEY_DATE',
        label: 'Show date',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'PERSIST_KEY_STEPS',
        label: 'Show step count',
        description: 'Falls back to sleep time overnight.',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'PERSIST_KEY_WEATHER',
        label: 'Weather dot',
        description: 'Temperature (°F) as a dot just inside the ring at its ' +
                     'minute mark — 72° sits at the 12-minute position. ' +
                     'Uses your location, and only while enabled.',
        defaultValue: false
      },
      {
        type: 'toggle',
        messageKey: 'PERSIST_KEY_BPM',
        label: 'BPM dot',
        description: 'Heart rate as a dot just inside the ring at its minute ' +
                     'mark, same idea as the weather dot. Watches with a ' +
                     'heart-rate sensor only.',
        defaultValue: false
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Style' },
      {
        type: 'select',
        messageKey: 'CLOCK_FONT',
        label: 'Clock font',
        defaultValue: 'mont',
        options: [
          { label: 'Montserrat', value: 'mont' },
          { label: 'Bitham (classic)', value: 'bitham' },
          { label: 'Roboto', value: 'roboto' },
          { label: 'LECO (oversized)', value: 'leco' }
        ]
      },
      {
        type: 'select',
        messageKey: 'THEME',
        label: 'Color theme',
        defaultValue: 'orange',
        options: [
          { label: 'Orange', value: 'orange' },
          { label: 'Black & White', value: 'bw' },
          { label: 'Green', value: 'green' },
          { label: 'Blue', value: 'blue' },
          { label: 'Purple', value: 'purple' },
          { label: 'Red', value: 'red' },
          { label: 'Teal', value: 'teal' },
          { label: 'Custom', value: 'custom' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Custom Theme Colors' },
      {
        type: 'text',
        defaultValue: 'Used when the color theme is set to Custom.'
      },
      {
        type: 'color',
        messageKey: 'PERSIST_KEY_CUSTOM_BG',
        label: 'Background',
        defaultValue: 0x000000,
        sunlight: false
      },
      {
        type: 'color',
        messageKey: 'PERSIST_KEY_CUSTOM_TIME',
        label: 'Time',
        defaultValue: 0xFFFFFF,
        sunlight: false
      },
      {
        type: 'color',
        messageKey: 'PERSIST_KEY_CUSTOM_DOT_ACTIVE',
        label: 'Active dots',
        defaultValue: 0xFF6A00,
        sunlight: false
      },
      {
        type: 'color',
        messageKey: 'PERSIST_KEY_CUSTOM_DOT_DIM',
        label: 'Dim dots',
        defaultValue: 0x555555,
        sunlight: false
      },
      {
        type: 'color',
        messageKey: 'PERSIST_KEY_CUSTOM_STEPS',
        label: 'Step count',
        defaultValue: 0xAAAAAA,
        sunlight: false
      },
      {
        type: 'color',
        messageKey: 'PERSIST_KEY_CUSTOM_DATE',
        label: 'Date',
        defaultValue: 0xAAAAAA,
        sunlight: false
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Options' },
      {
        type: 'toggle',
        messageKey: 'PERSIST_KEY_BOLD_TEXT',
        label: 'Bold text',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'PERSIST_KEY_BOLD_DOTS',
        label: 'Bold dots',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'PERSIST_KEY_MINMARKS',
        label: 'Hour marks',
        description: 'Shown when using bold dots.',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'PERSIST_KEY_FITDOTS',
        label: 'Fit dots on screen',
        description: 'Pebble Time 2 only. On, the ring is pulled in so edge ' +
                     'dots aren\'t clipped. Off gives the "zoom view": a ' +
                     'noticeably larger clock, with the ring cropping at the ' +
                     'left and right edges.',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'PERSIST_KEY_BATTERY',
        label: 'Battery level',
        description: 'Shown when using bold dots — outer dots thin out as ' +
                     'the battery drains.',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'PERSIST_KEY_CENTERED_TIME',
        label: 'Centered time display',
        description: '12-hour mode only: centers single-digit hours tightly ' +
                     'instead of reserving space so the colon doesn\'t shift ' +
                     'at 10:00.',
        defaultValue: false
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save Settings'
  }
];
