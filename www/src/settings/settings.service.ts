import { Injectable } from '@angular/core';

export class Option {
	label: string;
	value: string;
}

export class Setting {
	label: string;
	description?: string;
	placeholder?: string;
	name: string;
	type: string;
	mandatory: boolean;
	class?: string;
	value?: any;
	minimum?: number;
	maximum?: number;
	maxlength?: number;
	options?: Option[];
	badge?: string;
}

@Injectable()
export class SettingsService {
}
