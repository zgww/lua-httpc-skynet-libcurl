local httpc = require 'httpc'


local curl = require 'sncurl'


local e = curl.new()

curl.set_method(e, 'GET')
curl.set_param(e, 'name', pwd)
curl.set_param_file(e, 'name', filepath)
curl.set_param_file(e, 'name', filepath)


curl.request{
	url = '',
	method = 'GET',
	params = {
		name = {'path', ['content-type'] = '', filename = ''},
	},
	files = {
		name = path
	},
	headers = {
	}, 
	session = int,
	handle = int,
	save_to = string,  -- save response to path
}
yeild('CALL', session)


httpc.request('url', {
})
local res = httpc.request{'url',
	{name = 'zgww', pwd = 1},
	headers = {},
}
local res = httpc.get{
	'url', name = zz, pwd = dd,
	{love = '1'},
	{love = '3'},
	{love = '2'},

	headers = {
		['Content-Type'] : 'text/html'
	},
	cookies = {
		name = 'xx',
		xx = 'xx',
	},
}
local res = httpc.request{'url',
	{name = 'zgww'},
	{pwd = '123'},


	{
		key = v, key = v,
		{samekey = v}, 
		{samekey = v},
	},
	headers = {
		key = v,
		key = v,
	},
}
