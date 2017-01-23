import {
	Component,
	OnInit, OnChanges, SimpleChanges,
	Input
}                  from '@angular/core';
import { NgForm }  from '@angular/forms';

import { Setting } from './settings.service';

@Component( {
	selector: 'settings',
	templateUrl: 'tpl/settings.html',
	exportAs: 'settingsComponent'
} )

export class SettingsComponent implements OnInit, OnChanges {

	@Input( "settingsData" ) public settings: Setting[] = [];
	@Input( "settingsForm" ) public form: NgForm;

	public hasAdvancedSettings: boolean = false;
	public showAdvancedSettings: boolean = false;

	ngOnChanges( changes_: SimpleChanges ) {
		var me = this;
		// The hasAdvancedSettings property is used outside the component and has already been
		// checked by the renderer. Changing it in the same render run would cause an exception,
		// so change it afterwards (and unfortunately cause another update run).
		setTimeout( function() {
			for ( let setting of me.settings ) {
				if ( setting.class == 'advanced' ) {
					me.hasAdvancedSettings = true;
				}
			}
		}, 1 );
	};

	ngOnInit() {
	};

}
