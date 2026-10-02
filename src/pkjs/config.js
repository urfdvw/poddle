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
      },
      {
        type: 'select',
        messageKey: 'Theme',
        label: 'Theme',
        defaultValue: '0',
        capabilities: ['COLOR'],
        options: [
          { label: 'Black & white', value: '0' },
          { label: 'Color', value: '1' }
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
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Battery Saving'
      },
      {
        type: 'select',
        messageKey: 'UpdateSchedule',
        label: 'Update schedule',
        defaultValue: '0',
        options: [
          { label: 'Exact', value: '0' },
          { label: 'Random', value: '1' }
        ]
      },
      {
        type: 'input',
        messageKey: 'UpdateInterval',
        label: 'Update every [X] seconds',
        defaultValue: '1',
        description: 'A whole number from 1 to 59. Exact redraws every X seconds; ' +
          'Random waits 0.5\u00d7X plus a random 0 to X seconds each time (X on ' +
          'average). Only applies while the face shows seconds.',
        attributes: {
          type: 'number',
          min: 1,
          max: 59,
          step: 1,
          required: 'required'
        }
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
