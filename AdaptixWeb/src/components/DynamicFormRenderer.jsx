import React, { useState, useEffect } from 'react';
import { cn } from '../utils/cn';
import { Info, Upload, ChevronRight, ChevronDown } from 'lucide-react';

const Tabs = ({ children, defaultValue }) => {
  const [activeTab, setActiveTab] = useState(0);
  
  if (!children || children.length === 0) return null;

  return (
    <div className="flex flex-col h-full">
      <div className="flex space-x-1 border-b border-theme-glass-light overflow-x-auto custom-scrollbar">
        {children.map((child, idx) => (
          <button
            key={idx}
            onClick={() => setActiveTab(idx)}
            className={cn(
              "px-3 py-2 text-[10px] font-black uppercase tracking-widest border-b-2 transition-all whitespace-nowrap",
              activeTab === idx 
                ? "border-theme-accent text-theme-accent" 
                : "border-transparent text-theme-muted hover:text-theme-primary hover:border-theme-glass-light"
            )}
          >
            {child.props.label || `Tab ${idx + 1}`}
          </button>
        ))}
      </div>
      <div className="p-3 flex-1 overflow-y-auto custom-scrollbar">
        {children[activeTab]}
      </div>
    </div>
  );
};

const DynamicComponent = ({ component, value, onChange }) => {
  if (!component) return null;

  const { type, id, label, children, items, default: defaultValue, properties } = component;

  const handleChange = (newVal) => {
    if (id && onChange) {
      onChange(id, newVal);
    }
  };

  const currentValue = id ? (value[id] !== undefined ? value[id] : defaultValue) : undefined;

  switch (type) {
    case 'panel':
    case 'v_layout':
      return (
        <div className="space-y-2 w-full">
          {children && children.map((child, idx) => (
            <DynamicComponent 
              key={idx} 
              component={child} 
              value={value} 
              onChange={onChange} 
            />
          ))}
        </div>
      );

    case 'h_layout':
      return (
        <div className="flex space-x-2 w-full items-center">
          {children && children.map((child, idx) => (
            <div key={idx} className="flex-1">
              <DynamicComponent 
                component={child} 
                value={value} 
                onChange={onChange} 
              />
            </div>
          ))}
        </div>
      );

    case 'grid_layout':
      return (
        <div className="grid gap-2 w-full" style={{ gridTemplateColumns: 'repeat(12, minmax(0, 1fr))' }}>
          {children && children.map((child, idx) => {
            const props = child.properties || {};
            const row = props.grid_row !== undefined ? props.grid_row + 1 : 'auto';
            const col = props.grid_col !== undefined ? props.grid_col + 1 : 'auto';
            // Simple heuristic for col span if not provided: standard grid usually implies span. 
            // But here we rely on CSS grid placement if provided.
            // If col/row provided, use them. Note: Qt grids are 0-indexed, CSS 1-indexed.
            // Spans are often implicit or '1'.
            const rowSpan = props.grid_rowspan || 1;
            const colSpan = props.grid_colspan || 1;
            
            // Map 0-indexed column to 12-column grid approximation if needed, 
            // or just use grid-column-start/end
            // Actually, let's just render standard flow if no props, or apply style if props exist.
            const style = {};
            if (props.grid_row !== undefined) {
                style.gridRow = `${row} / span ${rowSpan}`;
                style.gridColumn = `${col} / span ${colSpan}`;
                
                // Hack for common 2-column or 3-column layouts in Qt to 12-col web grid
                // If colSpan is large (like covering rest), make it span full
                // This is tricky without knowing total columns. 
                // Let's rely on standard grid-auto-flow if checking props fails or assume a 2-col layout context.
                // Better approach: Use Flex rows for simpler layouts or just block.
                // But for now, let's try direct mapping.
                // Note: Qt GridLayout is relative. A widget at 0,0 and another at 0,1.
                // We don't know widths.
                // Fallback: If grid_layout, render as vertical stack if complex, or simple grid if manageable.
                // Let's ignore specific grid positioning for now and just stack, OR try a simple 2-col grid.
                // Reverting to simple stacking/flow for safety unless we have a robust grid system mapper.
                // For this MVP, I'll render children in flow, but respecting full width.
                delete style.gridRow;
                delete style.gridColumn;
                style.gridColumn = `span ${colSpan > 1 ? 12 : 6}`; // Heuristic
            }
            
            return (
              <div key={idx} style={style} className={colSpan > 1 ? "col-span-12" : "col-span-6"}>
                <DynamicComponent 
                  component={child} 
                  value={value} 
                  onChange={onChange} 
                />
              </div>
            );
          })}
        </div>
      );

    case 'input':
    case 'textline':
      return (
        <div className="space-y-1 text-left w-full">
          {label && <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">{label}</label>}
          <input
            type="text"
            value={currentValue || ''}
            onChange={(e) => handleChange(e.target.value)}
            placeholder={properties?.placeholder || ''}
            className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
          />
        </div>
      );

    case 'textarea':
    case 'textmulti':
      return (
        <div className="space-y-1 text-left w-full">
          {label && <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">{label}</label>}
          <textarea
            rows={5}
            value={currentValue || ''}
            onChange={(e) => handleChange(e.target.value)}
            placeholder={properties?.placeholder || ''}
            className="glass-input w-full font-mono py-2 px-3 text-theme-primary text-xs resize-y"
          />
        </div>
      );

    case 'spin':
      return (
        <div className="space-y-1 text-left w-full">
          {label && <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">{label}</label>}
          <input
            type="number"
            value={currentValue || 0}
            min={properties?.min}
            max={properties?.max}
            onChange={(e) => handleChange(parseInt(e.target.value))}
            className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
          />
        </div>
      );

    case 'select':
    case 'combo':
      return (
        <div className="space-y-1 text-left w-full">
          {label && <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">{label}</label>}
          <div className="relative">
            <select
              value={currentValue || (items && items.length > 0 ? items[0] : '')}
              onChange={(e) => handleChange(e.target.value)}
              className="glass-input w-full font-mono py-1.5 px-3 pr-8 text-theme-primary text-xs appearance-none bg-theme-glass-panel"
            >
              {items && items.map((item, idx) => (
                <option key={idx} value={item} className="bg-theme-glass-panel text-theme-primary">
                  {item}
                </option>
              ))}
            </select>
            <ChevronDown size={12} className="absolute right-2 top-1/2 -translate-y-1/2 text-theme-muted pointer-events-none" />
          </div>
        </div>
      );

    case 'checkbox':
    case 'check':
      return (
        <div className="flex items-center space-x-3 p-2 glass-card-sm border-theme-glass-light rounded-xl w-full">
          <input
            type="checkbox"
            id={`chk-${id || label}`}
            checked={!!currentValue}
            onChange={(e) => handleChange(e.target.checked)}
            className="w-3 h-3 rounded border-theme-glass text-theme-accent focus:ring-theme-accent/30"
          />
          {label && (
            <label htmlFor={`chk-${id || label}`} className="text-[10px] font-bold text-theme-primary uppercase tracking-widest cursor-pointer select-none">
              {label}
            </label>
          )}
        </div>
      );

    case 'label':
      return (
        <div className="text-[10px] text-theme-secondary font-bold tracking-wide py-1 w-full">
          {label}
        </div>
      );

    case 'spacer':
      return <div className="flex-1 min-h-[10px]" />;

    case 'file_selector':
      return (
        <div className="space-y-1 text-left w-full">
          {label && <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">{label}</label>}
          <div className="flex space-x-2">
            <input
              type="text"
              value={currentValue || ''}
              onChange={(e) => handleChange(e.target.value)}
              placeholder={properties?.placeholder || 'Select file...'}
              className="glass-input flex-1 font-mono py-1.5 px-3 text-theme-primary text-xs"
            />
            <button className="glass-btn p-1.5 text-theme-muted hover:text-theme-primary" title="Upload">
              <Upload size={14} />
            </button>
          </div>
        </div>
      );

    case 'groupbox':
      const isCheckable = properties?.checkable;
      const isChecked = isCheckable ? !!currentValue : true;
      
      return (
        <div className="border border-theme-glass-light rounded-xl p-3 space-y-2 bg-theme-glass/10 w-full">
          <div className="flex items-center space-x-2 mb-2">
            {isCheckable && (
              <input
                type="checkbox"
                checked={isChecked}
                onChange={(e) => handleChange(e.target.checked)}
                className="w-3 h-3 rounded border-theme-glass text-theme-accent focus:ring-theme-accent/30"
              />
            )}
            {label && <span className="text-[10px] font-black uppercase text-theme-muted tracking-widest">{label}</span>}
          </div>
          <div className={cn("space-y-2", !isChecked && "opacity-50 pointer-events-none")}>
            {children && children.map((child, idx) => (
              <DynamicComponent 
                key={idx} 
                component={child} 
                value={value} 
                onChange={onChange} 
              />
            ))}
          </div>
        </div>
      );

    case 'tabs':
      return (
        <Tabs>
          {children && children.map((child, idx) => (
            <div key={idx} label={child.label} className="h-full">
              {child.children && child.children.map((subChild, subIdx) => (
                <DynamicComponent 
                  key={subIdx} 
                  component={subChild} 
                  value={value} 
                  onChange={onChange} 
                />
              ))}
            </div>
          ))}
        </Tabs>
      );

    case 'tab_item':
        // Should be handled by 'tabs' parent, but if rendered directly:
        return (
            <div className="space-y-2">
                {children && children.map((child, idx) => (
                    <DynamicComponent key={idx} component={child} value={value} onChange={onChange} />
                ))}
            </div>
        );

    case 'date':
    case 'dateline':
      return (
        <div className="space-y-1 text-left w-full">
          {label && <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">{label}</label>}
          <input
            type="date"
            value={currentValue || ''}
            onChange={(e) => handleChange(e.target.value)}
            className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
          />
        </div>
      );

    case 'time':
    case 'timeline':
      return (
        <div className="space-y-1 text-left w-full">
          {label && <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">{label}</label>}
          <input
            type="time"
            step="1"
            value={currentValue || ''}
            onChange={(e) => handleChange(e.target.value)}
            className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
          />
        </div>
      );

    default:
      console.warn(`[DynamicForm] Unknown component type: ${type}`);
      return null;
  }
};

const DynamicFormRenderer = ({ schema, value, onChange }) => {
  if (!schema || !schema.root) return <div className="text-theme-muted text-xs p-4 flex items-center justify-center">No UI Definition</div>;

  // Initial population of defaults
  useEffect(() => {
    if (schema.fields) {
      const updates = {};
      let hasUpdates = false;
      Object.entries(schema.fields).forEach(([id, comp]) => {
        // Only set default if value is undefined AND default is present
        if (value[id] === undefined && comp.default !== undefined && comp.default !== null) {
          updates[id] = comp.default;
          hasUpdates = true;
        }
      });
      if (hasUpdates && onChange) {
        // We cannot simply call onChange here with a new object if onChange expects (id, val)
        // Check signature of onChange passed from parent.
        // In CreateListenerDialog, onChange is setConfig which expects a state update function or object.
        // If we assume it behaves like useState's setter:
        onChange(prev => ({ ...prev, ...updates }));
      }
    }
  }, [schema]); // Run once when schema changes

  return (
    <div className="dynamic-form-container h-full overflow-y-auto custom-scrollbar p-1">
      <DynamicComponent 
        component={schema.root} 
        value={value} 
        onChange={onChange} 
      />
    </div>
  );
};

export default DynamicFormRenderer;
