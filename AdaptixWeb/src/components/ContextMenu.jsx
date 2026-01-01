import React, { useEffect, useRef } from 'react';

const ContextMenu = ({ x, y, options, onClose }) => {
  const menuRef = useRef(null);

  useEffect(() => {
    const handleClickOutside = (event) => {
      if (menuRef.current && !menuRef.current.contains(event.target)) {
        onClose();
      }
    };
    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, [onClose]);

  return (
    <div 
      ref={menuRef}
      className="fixed z-50 bg-dark-700 border border-dark-600 rounded-lg shadow-2xl py-1 min-w-[160px] animate-in fade-in zoom-in duration-100"
      style={{ top: y, left: x }}
    >
      {options.map((option, idx) => (
        option.divider ? (
          <div key={idx} className="h-px bg-dark-600 my-1 mx-2" />
        ) : (
          <button
            key={idx}
            onClick={() => {
              option.onClick();
              onClose();
            }}
            className="w-full text-left px-4 py-2 text-xs text-gray-300 hover:bg-accent-primary hover:text-white transition-colors flex items-center space-x-2"
          >
            {option.icon && <option.icon className="w-3.5 h-3.5" />}
            <span>{option.label}</span>
          </button>
        )
      ))}
    </div>
  );
};

export default ContextMenu;
