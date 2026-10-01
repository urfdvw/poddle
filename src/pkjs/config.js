module.exports = [
  {
    type: 'heading',
    defaultValue: 'Poddle'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Display'
      },
      {
        type: 'select',
        messageKey: 'Orientation',
        label: 'Orientation',
        defaultValue: '0',
        options: [
          { label: 'Portrait', value: '0' },
          { label: 'Landscape', value: '1' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Progress bar'
      },
      {
        type: 'select',
        messageKey: 'ProgressMode',
        label: 'Bar measures',
        defaultValue: '1',
        options: [
          { label: 'Current minute', value: '0' },
          { label: 'Current hour', value: '1' }
        ]
      },
      {
        type: 'select',
        messageKey: 'LabelFormat',
        label: 'Labels show',
        defaultValue: '1',
        options: [
          { label: 'Segment start / end', value: '0' },
          { label: 'Elapsed / remaining', value: '1' }
        ]
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
