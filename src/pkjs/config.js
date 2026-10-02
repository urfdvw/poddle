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
        defaultValue: 'Custom period'
      },
      {
        type: 'text',
        defaultValue: 'Between the start and end time on the chosen days, the bar runs from ' +
          'start to end. Outside it, the Progress bar settings apply.'
      },
      {
        type: 'select',
        messageKey: 'PeriodRepeat',
        label: 'Active on',
        defaultValue: '0',
        options: [
          { label: 'Off', value: '0' },
          { label: 'One date', value: '1' },
          { label: 'Days of the week', value: '2' },
          { label: 'Every day', value: '3' }
        ]
      },
      {
        type: 'input',
        messageKey: 'PeriodDate',
        label: 'Date',
        defaultValue: '',
        attributes: { type: 'date' }
      },
      {
        type: 'checkboxgroup',
        messageKey: 'PeriodWeekdays',
        label: 'Days',
        defaultValue: [false, true, true, true, true, true, false],
        options: ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday']
      },
      {
        type: 'input',
        messageKey: 'PeriodStart',
        label: 'Start',
        defaultValue: '09:00',
        attributes: { type: 'time' }
      },
      {
        type: 'input',
        messageKey: 'PeriodEnd',
        label: 'End',
        defaultValue: '17:00',
        description: 'Must be after the start, on the same day.',
        attributes: { type: 'time' }
      },
      {
        type: 'select',
        messageKey: 'PeriodLabelFormat',
        label: 'Labels show',
        defaultValue: '1',
        options: [
          { label: 'Start / end', value: '0' },
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
        label: 'Redraw schedule',
        defaultValue: '0',
        options: [
          { label: 'Exact', value: '0' },
          { label: 'Random', value: '1' }
        ]
      },
      {
        type: 'input',
        messageKey: 'UpdateInterval',
        label: 'Redraw period (sec)',
        defaultValue: '1',
        description: 'A whole number from 1 to 60.',
        attributes: {
          type: 'number',
          min: 1,
          max: 60,
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
