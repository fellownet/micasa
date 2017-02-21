import {
	Component,
	Input,
	OnInit
}                  from '@angular/core';
import { NgForm }  from '@angular/forms';

import { Setting } from './settings.service';

@Component( {
	selector: 'settings',
	templateUrl: 'tpl/settings.html',
	exportAs: 'settingsComponent'
} )

export class SettingsComponent implements OnInit {

	@Input( "advanced" ) public showAdvancedSettings: boolean = false;

	@Input( "settings" ) public settings: Setting[] = [];
	@Input( "form" ) public form: NgForm;
	@Input( "values" ) public values: any;

	public ngOnInit() {
		this._sort();
	};

	public toggleAdvancedSettings() {
		this.showAdvancedSettings = ! this.showAdvancedSettings;
	};

	private _sort() {
		var me = this;
		this.settings.sort( function( a_: any, b_: any ): number {
			let a: number = ( 'sort' in a_ ? a_['sort'] : 0 );
			let b: number = ( 'sort' in b_ ? b_['sort'] : 0 );
			if ( a < b ) {
				return -1;
			} else if ( a > b ) {
				return 1;
			} else {
				return 0;
			}
		} );
	};
}
