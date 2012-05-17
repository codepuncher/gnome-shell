// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Shell = imports.gi.Shell;

const Config = imports.misc.config;
const Main = imports.ui.main;
const Params = imports.misc.params;


const STANDARD_STATUS_AREA_SHELL_IMPLEMENTATION = {
    'a11y': imports.ui.status.accessibility.ATIndicator,
    'volume': imports.ui.status.volume.Indicator,
    'battery': imports.ui.status.power.Indicator,
    'keyboard': imports.ui.status.keyboard.XKBIndicator,
    'userMenu': imports.ui.userMenu.UserMenuButton
};

if (Config.HAVE_BLUETOOTH)
    STANDARD_STATUS_AREA_SHELL_IMPLEMENTATION['bluetooth'] =
        imports.ui.status.bluetooth.Indicator;

try {
    STANDARD_STATUS_AREA_SHELL_IMPLEMENTATION['network'] =
        imports.ui.status.network.NMApplet;
} catch(e) {
    log('NMApplet is not supported. It is possible that your NetworkManager version is too old');
}


const DEFAULT_MODE = 'user';

const _modes = {
    'gdm': { hasOverview: false,
             hasAppMenu: false,
             showCalendarEvents: false,
             allowSettings: false,
             allowExtensions: false,
             allowKeybindingsWhenModal: true,
             hasRunDialog: false,
             hasWorkspaces: false,
             createSession: Main.createGDMSession,
             extraStylesheet: global.datadir + '/theme/gdm.css',
             statusArea: {
                 order: [
                     'a11y', 'display', 'keyboard',
                     'volume', 'battery', 'powerMenu'
                 ],
                 implementation: {
                     'a11y': imports.ui.status.accessibility.ATIndicator,
                     'volume': imports.ui.status.volume.Indicator,
                     'battery': imports.ui.status.power.Indicator,
                     'keyboard': imports.ui.status.keyboard.XKBIndicator,
                     'powerMenu': imports.gdm.powerMenu.PowerMenuButton
                 }
             },
             sessionType: Shell.SessionType.GDM },

    'user': { hasOverview: true,
              hasAppMenu: true,
              showCalendarEvents: true,
              allowSettings: true,
              allowExtensions: true,
              allowKeybindingsWhenModal: false,
              hasRunDialog: true,
              hasWorkspaces: true,
              createSession: Main.createUserSession,
              extraStylesheet: null,
              statusArea: {
                  order: [
                      'a11y', 'keyboard', 'volume', 'bluetooth',
                      'network', 'battery', 'userMenu'
                  ],
                  implementation: STANDARD_STATUS_AREA_SHELL_IMPLEMENTATION
              },
              sessionType: Shell.SessionType.USER }
};

function modeExists(mode) {
    let modes = Object.getOwnPropertyNames(_modes);
    return modes.indexOf(mode) != -1;
}

const SessionMode = new Lang.Class({
    Name: 'SessionMode',

    _init: function() {
        let params = _modes[global.session_mode];

        params = Params.parse(params, _modes[DEFAULT_MODE]);

        this._createSession = params.createSession;
        delete params.createSession;

        Lang.copyProperties(params, this);
    },

    createSession: function() {
        if (this._createSession)
            this._createSession();
    }
});
