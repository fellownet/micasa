import {
	Component,
	Input,
	OnChanges,
	SimpleChanges
}                      from '@angular/core';
import {
	NgForm,
	NgModel,
	ValidationErrors } from '@angular/forms';

import { Setting }     from './settings.service';

@Component( {
	selector: 'settings',
	templateUrl: 'tpl/settings.html',
	exportAs: 'settingsComponent'
} )

export class SettingsComponent implements OnChanges {

	@Input( "advanced" ) public showAdvancedSettings: boolean = false;

	@Input( "settings" ) public settings: Setting[] = [];
	@Input( "form" ) public form: NgForm;
	@Input( "values" ) public values: any;

	public ngOnChanges( changes_: SimpleChanges ) {
		if (
			'values' in changes_
			&& ! changes_.values.isFirstChange()
		) {
			this.form.form.reset( this.values );
		}
	};

	public toggleAdvancedSettings() {
		this.showAdvancedSettings = ! this.showAdvancedSettings;
	};

	public addToMultiSelect( value_: any, values_: any[], options_: any[] ) {
		options_.every( option_ => {
			if ( option_.value == value_ ) {
				values_.push( option_.value );
				return false;
			} else {
				return true;
			}
		} );
	};

	public removeFromMultiSelect( value_: any, values_: any[] ) {
		let pos: number = values_.indexOf( value_ );
		if ( pos >= 0 ) {
			values_.splice( pos, 1 );
		}
	};

}
