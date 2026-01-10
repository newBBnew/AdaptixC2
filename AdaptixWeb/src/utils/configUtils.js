export const processDynamicConfig = (data, schema) => {
  if (!schema?.properties) return data;
  const result = { ...data };
  
  Object.keys(schema.properties).forEach(key => {
    const prop = schema.properties[key];
    const value = result[key];
    
    if (value !== undefined && value !== null && value !== '') {
      if (prop.type === 'integer' || prop.type === 'number') {
        result[key] = Number(value);
      } else if (prop.type === 'boolean') {
        result[key] = String(value).toLowerCase() === 'true';
      }
    }
  });
  
  return result;
};
